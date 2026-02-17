#include "caparoc/modbus_connection.hpp"
#include <modbus/modbus.h>
#include <cstring>
#include <utility>
#include <stdexcept>
#include <unistd.h>
#include <cerrno>
#include <sys/socket.h>

namespace caparoc
{
    namespace
    {
        bool is_retryable_modbus_data_error(int error_code)
        {
            return error_code == EMBBADDATA ||
                   error_code == EMBMDATA ||
                   error_code == EMBBADCRC ||
                   error_code == EMBBADEXC ||
                   error_code == EMBUNKEXC;
        }

        void drain_socket_nonblocking(modbus_t *ctx)
        {
            const int socket_fd = modbus_get_socket(ctx);
            if (socket_fd < 0)
            {
                return;
            }

            uint8_t buffer[256];
            while (true)
            {
                const ssize_t bytes_read = recv(socket_fd, buffer, sizeof(buffer), MSG_DONTWAIT);
                if (bytes_read > 0)
                {
                    continue;
                }

                if (bytes_read == 0)
                {
                    return;
                }

                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    return;
                }

                return;
            }
        }

        template <typename Operation>
        bool execute_with_data_error_retry(modbus_t *ctx,
                                           std::string &last_error,
                                           const char *error_prefix,
                                           Operation operation)
        {
            constexpr int max_attempts = 2;
            for (int attempt = 0; attempt < max_attempts; ++attempt)
            {
                if (operation() != -1)
                {
                    return true;
                }

                const int error_code = errno;
                if (attempt == 0 && is_retryable_modbus_data_error(error_code))
                {
                    drain_socket_nonblocking(ctx);
                    continue;
                }

                last_error = std::string(error_prefix) + modbus_strerror(error_code);
                return false;
            }

            last_error = std::string(error_prefix) + "retry exhausted";
            return false;
        }
    }

    ModbusConnection::ModbusConnection(const std::string &ip_address, int port)
        : ctx_(nullptr), connected_(false)
    {
        ctx_ = modbus_new_tcp(ip_address.c_str(), port);
        if (!ctx_)
        {
            last_error_ = "Failed to create MODBUS context";
        }
    }

    ModbusConnection::~ModbusConnection()
    {
        if (connected_)
        {
            disconnect();
        }
        if (ctx_)
        {
            modbus_free(ctx_);
            ctx_ = nullptr;
        }
    }

    ModbusConnection::ModbusConnection(ModbusConnection &&other) noexcept
        : ctx_(other.ctx_), connected_(other.connected_),
          last_error_(std::move(other.last_error_))
    {
        other.ctx_ = nullptr;
        other.connected_ = false;
    }

    ModbusConnection &ModbusConnection::operator=(ModbusConnection &&other) noexcept
    {
        if (this != &other)
        {
            if (connected_)
            {
                disconnect();
            }
            if (ctx_)
            {
                modbus_free(ctx_);
            }

            ctx_ = other.ctx_;
            connected_ = other.connected_;
            last_error_ = std::move(other.last_error_);

            other.ctx_ = nullptr;
            other.connected_ = false;
        }
        return *this;
    }

    bool ModbusConnection::connect()
    {
        if (!ctx_)
        {
            last_error_ = "Invalid MODBUS context";
            return false;
        }

        if (connected_)
        {
            return true;
        }

        // Retry connection a few times if we get EWOULDBLOCK
        const int max_retries = 3;
        for (int attempt = 0; attempt < max_retries; ++attempt)
        {
            if (modbus_connect(ctx_) == 0)
            {
                connected_ = true;
                return true;
            }

            int err = errno;
            if (err == EWOULDBLOCK || err == EAGAIN)
            {
                // Non-blocking operation, wait a bit and retry
                if (attempt < max_retries - 1)
                {
                    usleep(100000);  // 100ms, try again
                    continue;
                }
            }

            last_error_ = std::string("Connection failed: ") + modbus_strerror(err);
            return false;
        }

        return false;
    }

    void ModbusConnection::disconnect()
    {
        if (ctx_ && connected_)
        {
            modbus_close(ctx_);
            connected_ = false;
        }
    }

    bool ModbusConnection::read_register(uint16_t address, uint16_t &value)
    {
        if (!connected_)
        {
            last_error_ = "Not connected";
            return false;
        }

        return execute_with_data_error_retry(ctx_, last_error_, "Read failed: ",
                                             [this, address, &value]()
                                             { return modbus_read_registers(ctx_, address, 1, &value); });
    }

    bool ModbusConnection::read_registers(uint16_t address, uint16_t count, uint16_t *values)
    {
        if (!connected_)
        {
            last_error_ = "Not connected";
            return false;
        }

        return execute_with_data_error_retry(ctx_, last_error_, "Read failed: ",
                                             [this, address, count, values]()
                                             { return modbus_read_registers(ctx_, address, count, values); });
    }

    bool ModbusConnection::write_register(uint16_t address, uint16_t value)
    {
        if (!connected_)
        {
            last_error_ = "Not connected";
            return false;
        }

        return execute_with_data_error_retry(ctx_, last_error_, "Write failed: ",
                                             [this, address, value]()
                                             { return modbus_write_register(ctx_, address, value); });
    }

    bool ModbusConnection::write_registers(uint16_t address, uint16_t count, const uint16_t *values)
    {
        if (!connected_)
        {
            last_error_ = "Not connected";
            return false;
        }

        return execute_with_data_error_retry(ctx_, last_error_, "Write failed: ",
                                             [this, address, count, values]()
                                             { return modbus_write_registers(ctx_, address, count, values); });
    }

    bool ModbusConnection::read_coil(uint16_t address, bool &value)
    {
        if (!connected_)
        {
            last_error_ = "Not connected";
            return false;
        }

        uint8_t coil_value = 0;
        const bool success = execute_with_data_error_retry(ctx_, last_error_, "Read coil failed: ",
                                                           [this, address, &coil_value]()
                                                           { return modbus_read_bits(ctx_, address, 1, &coil_value); });
        if (!success)
        {
            return false;
        }

        value = (coil_value != 0);
        return true;
    }

    bool ModbusConnection::read_coils(uint16_t address, uint16_t count, uint8_t *values)
    {
        if (!connected_)
        {
            last_error_ = "Not connected";
            return false;
        }

        return execute_with_data_error_retry(ctx_, last_error_, "Read coils failed: ",
                                             [this, address, count, values]()
                                             { return modbus_read_bits(ctx_, address, count, values); });
    }

    bool ModbusConnection::write_coil(uint16_t address, bool state)
    {
        if (!connected_)
        {
            last_error_ = "Not connected";
            return false;
        }

        return execute_with_data_error_retry(ctx_, last_error_, "Write coil failed: ",
                                             [this, address, state]()
                                             { return modbus_write_bit(ctx_, address, state ? 1 : 0); });
    }

    bool ModbusConnection::write_coils(uint16_t address, uint16_t count, const uint8_t *values)
    {
        if (!connected_)
        {
            last_error_ = "Not connected";
            return false;
        }

        return execute_with_data_error_retry(ctx_, last_error_, "Write coils failed: ",
                                             [this, address, count, values]()
                                             { return modbus_write_bits(ctx_, address, count, values); });
    }

    // Add to implementation
    bool ModbusConnection::set_slave_id(int slave_id)
    {
        if (!ctx_)
        {
            last_error_ = "Invalid MODBUS context";
            return false;
        }

        if (modbus_set_slave(ctx_, slave_id) == -1)
        {
            last_error_ = std::string("Set slave failed: ") + modbus_strerror(errno);
            return false;
        }

        return true;
    }

    std::string ModbusConnection::get_last_error() const
    {
        return last_error_;
    }

    void ModbusConnection::set_response_timeout(uint32_t seconds, uint32_t microseconds)
    {
        if (ctx_)
        {
            modbus_set_response_timeout(ctx_, seconds, microseconds);
        }
    }

} // namespace caparoc
