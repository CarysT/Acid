#include "ButtonMouse.hpp"

namespace acid
{
ButtonMouse::ButtonMouse(const MouseButton &button) :
	m_button(button)
{
	Mouse::Get()->OnButton().Add([this](MouseButton button, InputAction action, BitMask<InputMod> mods)
	{
		if (button == m_button)
		{
			m_onButton(action, mods);
		}
	}, std::ref(*this));
}

bool ButtonMouse::IsDown() const
{
	return Mouse::Get()->GetButton(m_button) != InputAction::Release;
}
}
