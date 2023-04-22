#pragma once

#include <cassert>

#include "hdi08.h"
#include "logging.h"

namespace mc68k
{
	constexpr uint32_t g_readTimeoutCycles = 50;

	template <uint32_t Base, uint32_t Size> Hdi08<Base, Size>::Hdi08()
	{
		setWriteTxCallback(nullptr);
		setWriteIrqCallback(nullptr);
		setReadIsrCallback(nullptr);

		write8(toPeriphAddress(Address::IVR), 0xf);
	}

	template <uint32_t Base, uint32_t Size> void Hdi08<Base, Size>::pollTx(std::deque<uint32_t>& _dst)
	{
		std::swap(_dst, m_txData);
	}

	template <uint32_t Base, uint32_t Size> uint8_t Hdi08<Base, Size>::read8(PeriphAddress _addr)
	{
		const auto a = static_cast<Address>(_addr);

		switch (a)
		{
		case Address::ICR:
			return icr();
		case Address::ISR:
			return isr();
		case Address::TXH:	return readRX(WordFlags::H);
		case Address::TXM:	return readRX(WordFlags::M);
		case Address::TXL:	return readRX(WordFlags::L);
		case Address::CVR:
			return PeripheralBase<Base,Size>::read8(_addr);
		}
		const auto r = PeripheralBase<Base, Size>::read8(_addr);
//		MCLOG("read8 addr=" << MCHEXN(_addr, 8));
		return r;
	}

	template <uint32_t Base, uint32_t Size> uint16_t Hdi08<Base, Size>::read16(PeriphAddress _addr)
	{
		const auto a = toAddress(_addr);
		switch (a)
		{
		case Address::Unused4:
			return read8(toPeriphAddress(Address::TXH));
		case Address::TXM:
		{
			uint16_t r = (static_cast<uint16_t>(read8(toPeriphAddress(Address::TXM))) << static_cast<uint16_t>(8));
			r |= static_cast<uint16_t>(read8(toPeriphAddress(Address::TXL)));
			return r;
		}
		}
		MCLOG("read16 addr=" << MCHEXN(_addr, 8));
		return PeripheralBase<Base, Size>::read16(_addr);
	}

	template <uint32_t Base, uint32_t Size> void Hdi08<Base, Size>::write8(PeriphAddress _addr, uint8_t _val)
	{
		PeripheralBase<Base, Size>::write8(_addr, _val);

		const auto a = toAddress(_addr);

		switch (a)
		{
		case Address::ISR:
//			MCLOG("HDI08 ISR set to " << MCHEXN(_val,2));
			return;
		case Address::ICR:
//			MCLOG("HDI08 ICR set to " << MCHEXN(_val,2));
			if (_val & Init)
			{
				MCLOG("HDI08 Initialization, HREQ=" << (_val & Rreq) << ", TREQ=" << (_val & Treq));
			}
			return;
		case Address::CVR:
			if (_val & Hc)
			{
				const auto addr = static_cast<uint8_t>((_val & Hv) << 1);
				//				MCLOG("HDI08 Host Vector Interrupt Request, interrupt vector = " << MCHEXN(addr, 2));
				m_writeIrqCallback(addr);

				const auto val = read8(toPeriphAddress(Address::CVR));
				PeripheralBase<Base, Size>::write8(toPeriphAddress(Address::CVR), val & ~Hc);

//				write8(_addr, _val & ~Hc);
			}
			return;
		case Address::TXH:	writeTX(WordFlags::H, _val);	return;
		case Address::TXM:	writeTX(WordFlags::M, _val);	return;
		case Address::TXL:	writeTX(WordFlags::L, _val);	return;
		}
		MCLOG("write8 addr=" << MCHEXN(_addr, 8) << ", val=" << MCHEXN(static_cast<int>(_val), 2));
	}

	template <uint32_t Base, uint32_t Size> void Hdi08<Base, Size>::write16(PeriphAddress _addr, uint16_t _val)
	{
		PeripheralBase<Base, Size>::write16(_addr, _val);

		const auto a = toAddress(_addr);

		switch (a)
		{
		case Address::Unused4:
			write8(toPeriphAddress(Address::TXH), _val & 0xff);
			return;
		case Address::TXM:
			write8(toPeriphAddress(Address::TXM), _val >> 8);
			write8(toPeriphAddress(Address::TXL), _val & 0xff);
			break;
		default:
			MCLOG("write16 addr=" << MCHEXN(_addr, 8) << ", val=" << MCHEXN(_val, 4));
			break;
		}
	}

	template <uint32_t Base, uint32_t Size> bool Hdi08<Base, Size>::pollInterruptRequest(uint8_t& _addr)
	{
		if (m_pendingInterruptRequests.empty())
			return false;

		_addr = m_pendingInterruptRequests.front();

		m_pendingInterruptRequests.pop_front();

		return true;
	}

	template <uint32_t Base, uint32_t Size> void Hdi08<Base, Size>::writeRx(uint32_t _word)
	{
//		MCLOG("HDI writeRX=" << HEX(_word));

		m_rxData.push_back(_word);

		const auto s = isr();

		if (!(s & Rxdf))
			pollRx();
	}

	template <uint32_t Base, uint32_t Size> void Hdi08<Base, Size>::exec(uint32_t _deltaCycles)
	{
		PeripheralBase<Base, Size>::exec(_deltaCycles);

		auto isr = PeripheralBase<Base, Size>::read8(toPeriphAddress(Address::ISR));//Hdi08<Base, Size>::isr();

		if (!(isr & Rxdf))
			return;

		if (m_rxData.empty())
			return;

		m_readTimeoutCycles += _deltaCycles;

		if (m_readTimeoutCycles >= g_readTimeoutCycles)
		{
			MCLOG("HDI RX read timeout on byte " << MCHEXN(m_rxd, 2));
			isr &= ~Rxdf;
			write8(toPeriphAddress(Address::ISR), isr);
			pollRx();
		}
	}

	template <uint32_t Base, uint32_t Size> bool Hdi08<Base, Size>::canReceiveData()
	{
		return (PeripheralBase<Base, Size>::read8(toPeriphAddress(Address::ISR)) & Rxdf) == 0;
	}

	template <uint32_t Base, uint32_t Size> void Hdi08<Base, Size>::setWriteTxCallback(const CallbackWriteTx& _writeTxCallback)
	{
		if (_writeTxCallback)
		{
			m_writeTxCallback = _writeTxCallback;
		}
		else
		{
			m_writeTxCallback = [this](const uint32_t _word)
			{
				m_txData.push_back(_word);
			};
		}
	}

	template <uint32_t Base, uint32_t Size> void Hdi08<Base, Size>::setWriteIrqCallback(const CallbackWriteIrq& _writeIrqCallback)
	{
		if (_writeIrqCallback)
		{
			m_writeIrqCallback = _writeIrqCallback;
		}
		else
		{
			m_writeIrqCallback = [this](uint8_t _irq)
			{
				m_pendingInterruptRequests.push_back(_irq);
			};
		}
	}

	template <uint32_t Base, uint32_t Size> void Hdi08<Base, Size>::setReadIsrCallback(const CallbackReadIsr& _readIsrCallback)
	{
		if (_readIsrCallback)
		{
			m_readIsrCallback = _readIsrCallback;
		}
		else
		{
			m_readIsrCallback = [](const uint8_t _isr) { return _isr; };
		}
	}

	template <uint32_t Base, uint32_t Size> void Hdi08<Base, Size>::writeTX(WordFlags _index, uint8_t _val)
	{
		m_txBytes[static_cast<uint32_t>(_index)] = _val;

		const auto lastWritten = m_writtenFlags;
		addIndex(m_writtenFlags, _index);
		assert(lastWritten != m_writtenFlags && "byte written twice!");

#if 1
		if (m_writtenFlags != WordFlags::Mask)
			return;
#else
		const auto le = littleEndian();
		if ((le && _index != WordFlags::L) || (!le && _index != WordFlags::H))
			return;
#endif

		const uint32_t h = m_txBytes[0];
		const uint32_t m = m_txBytes[1];
		const uint32_t l = m_txBytes[2];

		const auto word = littleEndian() ?
			l << 16 | m << 8 | h :
			h << 16 | m << 8 | l;

		m_writeTxCallback(word);

		//		MCLOG("HDI TX: " << MCHEXN(word, 6));

		m_txBytes.fill(0);

		m_writtenFlags = WordFlags::None;
	}

	template <uint32_t Base, uint32_t Size> typename Hdi08<Base, Size>::WordFlags Hdi08<Base, Size>::makeMask(WordFlags _index)
	{
		return static_cast<WordFlags>(1 << static_cast<uint32_t>(_index));
	}

	template <uint32_t Base, uint32_t Size> void Hdi08<Base, Size>::addIndex(WordFlags& _existing, WordFlags _indexToAdd)
	{
		_existing = static_cast<WordFlags>(static_cast<uint32_t>(_existing) | static_cast<uint32_t>(makeMask(_indexToAdd)));
	}

	template <uint32_t Base, uint32_t Size> void Hdi08<Base, Size>::removeIndex(WordFlags& _existing, WordFlags _indexToRemove)
	{
		_existing = static_cast<WordFlags>(static_cast<uint32_t>(_existing) & ~static_cast<uint32_t>(makeMask(_indexToRemove)));
	}

	template <uint32_t Base, uint32_t Size> uint8_t Hdi08<Base, Size>::littleEndian()
	{
		return read8(toPeriphAddress(Address::ICR)) & static_cast<uint8_t>(Hlend);
	}

	template <uint32_t Base, uint32_t Size> uint8_t Hdi08<Base, Size>::readRX(WordFlags _index)
	{
		const auto hasRX = isr() & Rxdf;

		if (!hasRX)
		{
			m_rxEmptyCallback(true);

			const auto s = isr();

			if (!(s & Rxdf))
			{
				//				MCLOG("Empty read of RX");
				return 0;
			}
		}

		const auto word = m_rxd;

		std::array<uint8_t, 3> bytes{};

		const auto le = littleEndian();

		auto pop = [&]()
		{
			write8(toPeriphAddress(Address::ISR), isr() & ~(Rxdf));
			m_rxEmptyCallback(false);
		};

		if (le)
		{
			bytes[0] = (word) & 0xff;
			bytes[1] = (word >> 8) & 0xff;
			bytes[2] = (word >> 16) & 0xff;

			if (_index == WordFlags::H)
				pop();
		}
		else
		{
			bytes[0] = (word >> 16) & 0xff;
			bytes[1] = (word >> 8) & 0xff;
			bytes[2] = (word) & 0xff;

			if (_index == WordFlags::L)
				pop();
		}

		removeIndex(m_readFlags, _index);

		return bytes[static_cast<uint32_t>(_index)];
	}

	template <uint32_t Base, uint32_t Size> bool Hdi08<Base, Size>::pollRx()
	{
		if (m_rxData.empty())
			return false;

		m_readTimeoutCycles = 0;
		m_rxd = m_rxData.front();
		m_rxData.pop_front();

		auto isr = Hdi08<Base, Size>::isr();

		write8(toPeriphAddress(Address::ISR), isr | Rxdf);
		m_readFlags = WordFlags::Mask;

		if (isr & Hreq)
		{
			const auto ivr = read8(toPeriphAddress(Address::IVR));
			assert(false);
		}

		return true;
	}
}
