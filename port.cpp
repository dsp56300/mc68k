#include "port.h"

namespace mc68k
{
	Port::Port() : m_dirChangeCallback([](const Port&){}), m_writeTXCallback([](const Port&) {})
	{
	}

	void Port::writeTX(const uint8_t _data)
	{
		// only write pins that are enabled and that are set to output
		const auto mask = m_direction & m_enabledPins;
		m_data &= ~mask;
		m_data |= _data & mask;
		++m_writeCounter;

		m_writeTXCallback(*this);
	}

	void Port::writeRX(const uint8_t _data)
	{
		// only write pins that are enabled and that are set to input
		const auto mask = (~m_direction) & m_enabledPins;
		m_data &= ~mask;
		m_data |= _data & mask;
	}

	void Port::setDirectionChangeCallback(const std::function<void(const Port&)>& _func)
	{
		m_dirChangeCallback = _func;
		if(!m_dirChangeCallback)
			m_dirChangeCallback = [](const Port&){};
	}

	void Port::setWriteTXCallback(const std::function<void(const Port&)>& _func)
	{
		m_writeTXCallback = _func;
		if(!m_writeTXCallback)
			m_writeTXCallback = [](const Port&){};
	}
}
