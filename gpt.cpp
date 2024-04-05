#include "gpt.h"

#include "logging.h"
#include "mc68k.h"

namespace mc68k
{
	namespace
	{
		void funcTimerOverflow(Gpt* _gpt)
		{
			_gpt->timerOverflow();
		}
		void funcTimerNoOverflow(Gpt*)
		{
		}
		constexpr uint16_t g_tmsk1_oci1Bit = 11;
		constexpr uint16_t g_tmsk1_oci2Bit = 12;
		constexpr uint16_t g_tmsk1_oci3Bit = 13;
		constexpr uint16_t g_tmsk1_oci4Bit = 14;

		constexpr uint16_t g_tmsk1_oci1Mask = (1<<g_tmsk1_oci1Bit);
		constexpr uint16_t g_tmsk1_oci2Mask = (1<<g_tmsk1_oci2Bit);
		constexpr uint16_t g_tmsk1_oci3Mask = (1<<g_tmsk1_oci3Bit);
		constexpr uint16_t g_tmsk1_oci4Mask = (1<<g_tmsk1_oci4Bit);

		constexpr uint8_t g_vba_oc1 = 0b100;
		constexpr uint8_t g_vba_oc2 = g_vba_oc1 + 1;
		constexpr uint8_t g_vba_oc3 = g_vba_oc1 + 2;
		constexpr uint8_t g_vba_oc4 = g_vba_oc1 + 3;

		constexpr uint16_t g_tflg1_oc1fBit = 11;
		constexpr uint16_t g_tflg1_oc2fBit = 12;
		constexpr uint16_t g_tflg1_oc3fBit = 13;
		constexpr uint16_t g_tflg1_oc4fBit = 14;

		constexpr uint16_t g_tflg1_oc1fMask = (1<<g_tflg1_oc1fBit);
		constexpr uint16_t g_tflg1_oc2fMask = (1<<g_tflg1_oc2fBit);
		constexpr uint16_t g_tflg1_oc3fMask = (1<<g_tflg1_oc3fBit);
		constexpr uint16_t g_tflg1_oc4fMask = (1<<g_tflg1_oc4fBit);

		constexpr uint16_t g_tflg1_ocfAll = g_tflg1_oc1fMask | g_tflg1_oc2fMask | g_tflg1_oc3fMask | g_tflg1_oc4fMask;

	}

	Gpt::Gpt(Mc68k& _mc68k): m_mc68k(_mc68k)
	{
		m_timerFuncs[0] = &funcTimerNoOverflow;
		m_timerFuncs[1] = &funcTimerOverflow;

		write16(PeriphAddress::Tmsk1, 0);
		write16(PeriphAddress::Tflg1, 0);

		write16(PeriphAddress::Toc1, 0xffff);
		write16(PeriphAddress::Toc2, 0xffff);
		write16(PeriphAddress::Toc3, 0xffff);
		write16(PeriphAddress::Toc4, 0xffff);
	}

	void Gpt::write8(PeriphAddress _addr, const uint8_t _val)
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
		default:
			MCLOG("read8 addr=" << MCHEXN(_addr, 8));
		}

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
//		default:
//			MCLOG("write16 addr=" << MCHEXN(_addr, 8) << ", val=" << MCHEXN(_val,4));
		}
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
		case PeriphAddress::Tcnt:
			{
				const auto r = (m_mc68k.getCycles() >> 2) & 0xffff;
//				MCLOG("Read TCNT=" << MCHEXN(r,4) << " at PC=" << m_mc68k.getPC());
				return static_cast<uint16_t>(r);
			}
		}

//		MCLOG("read16 addr=" << MCHEXN(_addr, 8));

		return PeripheralBase::read16(_addr);
	}

	void Gpt::injectInterrupt(uint8_t _vba)
	{
		const auto icr = read16(PeriphAddress::Icr);
		const auto level = static_cast<uint8_t>((icr >> 8) & 3);
		const auto msb = icr & 0xf0;
		const auto vba = static_cast<uint8_t>(msb | _vba);

		if(!m_mc68k.hasPendingInterrupt(vba, level))
			m_mc68k.injectInterrupt(vba, level);
	}

	void Gpt::exec(const uint32_t _deltaCycles)
	{
		// this code will jump to member function 'timerOverflow' if the timer overflowed, jumps to a nop otherwise
		const uint16_t tcnt = read16(PeriphAddress::Tcnt);

		const int timerDiff = static_cast<int>(tcnt) - static_cast<int>(m_prevTcnt);

		m_prevTcnt = tcnt;

		const uint32_t overflow = static_cast<uint32_t>(timerDiff) >> 31;

		m_timerFuncs[overflow](this);

		PeripheralBase::exec(_deltaCycles);
	}

	void Gpt::timerOverflow()
	{
		const auto tmsk = read16(PeriphAddress::Tmsk1);

		write16(PeriphAddress::Tflg1, read16(PeriphAddress::Tflg1) | g_tflg1_ocfAll);

		if(tmsk & g_tmsk1_oci1Mask)		injectInterrupt(g_vba_oc1);
		if(tmsk & g_tmsk1_oci2Mask)		injectInterrupt(g_vba_oc2);
		if(tmsk & g_tmsk1_oci3Mask)		injectInterrupt(g_vba_oc3);
		if(tmsk & g_tmsk1_oci4Mask)		injectInterrupt(g_vba_oc4);
	}
}
