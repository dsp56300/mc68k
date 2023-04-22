#include "gpt.h"

#include "logging.h"
#include "mc68k.h"

namespace mc68k
{
	void Gpt::write8(PeriphAddress _addr, uint8_t _val)
	{
		PeripheralBase::write8(_addr, _val);

		switch (_addr)
		{
		case PeriphAddress::DdrGp:
			m_portGP.setDirection(_val);
			return;
		case PeriphAddress::PortGp:
//			LOG("Set PortGP to " << HEXN(_val,2));
			m_portGP.writeTX(_val);
			return;
		}

		MCLOG("write8 addr=" << MCHEXN(_addr, 8) << ", val=" << MCHEXN(static_cast<int>(_val),2));
	}

	uint8_t Gpt::read8(PeriphAddress _addr)
	{
		switch (_addr)
		{
		case PeriphAddress::PortGp:
			return m_portGP.read();
		case PeriphAddress::Tcnt:
			return static_cast<uint8_t>(read16(PeriphAddress::Tcnt) >> 8);
		case PeriphAddress::TcntLSB:
			return static_cast<uint8_t>(read16(PeriphAddress::Tcnt) & 0xff);
		}

		MCLOG("read8 addr=" << MCHEXN(_addr, 8));

		return PeripheralBase::read8(_addr);
	}

	void Gpt::write16(PeriphAddress _addr, uint16_t _val)
	{
		PeripheralBase::write16(_addr, _val);

		switch (_addr)
		{
		case PeriphAddress::DdrGp:
			m_portGP.setDirection(_val & 0xff);
			m_portGP.writeTX(_val>>8);
			break;
		}
		MCLOG("write16 addr=" << MCHEXN(_addr, 8) << ", val=" << MCHEXN(_val,4));
	}

	uint16_t Gpt::read16(PeriphAddress _addr)
	{
		switch (_addr)
		{
		case PeriphAddress::DdrGp:
			{
				const uint16_t dir = m_portGP.getDirection();
				const uint16_t data = m_portGP.read();
				return dir << 8 | data;
			}
			break;
		case PeriphAddress::Tcnt:
			{
				const auto r = (m_mc68k.getCycles() >> 2) & 0xffff;
				MCLOG("Read TCNT=" << MCHEXN(r,4) << " at PC=" << m_mc68k.getPC());
				return static_cast<uint16_t>(r);
			}
		}

		MCLOG("read16 addr=" << MCHEXN(_addr, 8));

		return PeripheralBase::read16(_addr);
	}
}
