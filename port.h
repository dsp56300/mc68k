#pragma once

#include <cstdint>
#include <functional>

namespace mc68k
{
	class Port
	{
	public:
		Port();

		uint8_t getDirection() const
		{
			return m_direction;
		}

		void setDirection(const uint8_t _dir)
		{
			if(m_direction == _dir)
				return;

			// A pin that turns into an output drives whatever the data register holds, including a value
			// written to it while the pin was still an input - firmware writes the level first and enables
			// the driver afterwards to avoid a glitch. writeTX keeps that value for exactly this moment.
			const auto newOutputs = static_cast<uint8_t>(_dir & ~m_direction & m_enabledPins);
			m_data = static_cast<uint8_t>((m_data & ~newOutputs) | (m_dataTX & newOutputs));

			m_direction = _dir;
			m_dirChangeCallback(*this);
		}

		void writeTX(uint8_t _data);
		void writeRX(uint8_t _data);

		uint8_t read() const
		{
			return m_readRXCallback(*this, m_data);
		}

		void enablePins(uint8_t _pins)
		{
			m_enabledPins = _pins;
		}

		uint32_t getWriteCounter() const
		{
			return m_writeCounter;
		}

		uint8_t bittest(uint32_t _bit) const
		{
			return read() & (1<<_bit);
		}

		void setBitRX(uint32_t _bit)
		{
			writeRX(read() | static_cast<uint8_t>(1 << _bit));
		}

		void clearBitRX(uint32_t _bit)
		{
			writeRX(read() & ~(1<<_bit));
		}

		void setDirectionChangeCallback(const std::function<void(const Port&)>& _func);
		void setWriteTXCallback(const std::function<void(const Port&)>& _func);
		void setReadRXCallback(const std::function<uint8_t(const Port&, uint8_t)>& _func);

	private:
		uint8_t m_direction = 0;		// 0 = input, 1 = output
		uint8_t m_enabledPins = 0xff;
		uint8_t m_data = 0;
		uint8_t m_dataTX = 0;	// the data register itself, written regardless of the direction of a pin
		uint32_t m_writeCounter = 0;
		std::function<void(const Port&)> m_dirChangeCallback;
		std::function<void(const Port&)> m_writeTXCallback;
		std::function<uint8_t(const Port&, uint8_t)> m_readRXCallback;
	};
}
