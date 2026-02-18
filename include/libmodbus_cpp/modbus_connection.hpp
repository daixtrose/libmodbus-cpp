#pragma once

#include <memory>
#include <string>
#include <cstdint>

// Forward declare modbus_t to avoid exposing libmodbus in header
struct _modbus;
typedef struct _modbus modbus_t;

namespace libmodbus_cpp
{
    inline namespace v1
    {

    /**
     * @brief RAII wrapper for MODBUS TCP connection
     *
     * This class provides a modern C++23 interface to libmodbus with automatic
     * resource management.
     */
    class ModbusConnection
    {
    public:
        /**
         * @brief Construct a new MODBUS TCP connection
         *
         * @param ip_address IP address of the LIBMODBUS_CPP device
         * @param port TCP port (default: 502)
         */
        explicit ModbusConnection(const std::string &ip_address, int port = 502);

        /**
         * @brief Destroy the connection and cleanup resources
         */
        ~ModbusConnection();

        // Disable copy
        ModbusConnection(const ModbusConnection &) = delete;
        ModbusConnection &operator=(const ModbusConnection &) = delete;

        // Enable move
        ModbusConnection(ModbusConnection &&other) noexcept;
        ModbusConnection &operator=(ModbusConnection &&other) noexcept;

        /**
         * @brief Connect to the MODBUS device
         *
         * @return true if connection successful
         * @return false if connection failed
         */
        bool connect();

        /**
         * @brief Disconnect from the MODBUS device
         */
        void disconnect();

        /**
         * @brief Check if currently connected
         *
         * @return true if connected
         * @return false if not connected
         */
        bool is_connected() const noexcept { return connected_; }

        /**
         * @brief Read a single holding register
         *
         * @param address Register address
         * @param value Output value
         * @return true if read successful
         * @return false if read failed
         */
        bool read_register(uint16_t address, uint16_t &value);

        /**
         * @brief Read multiple holding registers
         *
         * @param address Starting register address
         * @param count Number of registers to read
         * @param values Output array (must be at least count elements)
         * @return true if read successful
         * @return false if read failed
         */
        bool read_registers(uint16_t address, uint16_t count, uint16_t *values);

        /**
         * @brief Write a single holding register
         *
         * @param address Register address
         * @param value Value to write
         * @return true if write successful
         * @return false if write failed
         */
        bool write_register(uint16_t address, uint16_t value);

        /**
         * @brief Write multiple holding registers
         *
         * @param address Starting register address
         * @param count Number of registers to write
         * @param values Input array (must be at least count elements)
         * @return true if write successful
         * @return false if write failed
         */
        bool write_registers(uint16_t address, uint16_t count, const uint16_t *values);

        /**
         * @brief Read a single coil status
         *
         * @param address Coil address (0-7 for relay channels)
         * @param value Output value (true=on, false=off)
         * @return true if read successful
         * @return false if read failed
         */
        bool read_coil(uint16_t address, bool &value);

        /**
         * @brief Read multiple coil statuses
         *
         * @param address Starting coil address
         * @param count Number of coils to read
         * @param values Output array (must be at least count elements)
         * @return true if read successful
         * @return false if read failed
         */
        bool read_coils(uint16_t address, uint16_t count, uint8_t *values);

        /**
         * @brief Write a single coil (relay control)
         *
         * @param address Coil address (0-7 for relay channels, 0xFF for all relays)
         * @param state true to turn on, false to turn off
         * @return true if write successful
         * @return false if write failed
         */
        bool write_coil(uint16_t address, bool state);

        /**
         * @brief Write multiple coils (relay control)
         *
         * @param address Starting coil address
         * @param count Number of coils to write
         * @param values Input array (must be at least count elements)
         * @return true if write successful
         * @return false if write failed
         */
        bool write_coils(uint16_t address, uint16_t count, const uint8_t *values);

        // Add to header
        /**
         * @brief Set the slave/unit ID for Modbus communication
         *
         * @param slave_id Slave ID (default is typically 1 for Waveshare devices)
         * @return true if successful
         * @return false if failed
         */
        bool set_slave_id(int slave_id);

        /**
         * @brief Get the last error message
         *
         * @return std::string Error message
         */
        std::string get_last_error() const;

        /**
         * @brief Set response timeout
         *
         * @param seconds Timeout in seconds
         * @param microseconds Additional microseconds
         */
        void set_response_timeout(uint32_t seconds, uint32_t microseconds);

        /**
         * @brief Get the raw modbus context (for advanced use)
         *
         * @return modbus_t* Raw context pointer
         */
        modbus_t *get_context() { return ctx_; }

    private:
        modbus_t *ctx_;
        bool connected_;
        std::string last_error_;
    };

    } // namespace v1
} // namespace libmodbus_cpp
