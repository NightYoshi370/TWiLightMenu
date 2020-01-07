/*
	button.cpp
	Copyright (C) 2007 Acekard, www.acekard.com
	Copyright (C) 2007-2009 somebody
	Copyright (C) 2009 yellow wood goblin

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "ui.h"
#include "button.h"
#include "font/fontfactory.h"
#include "window.h"

namespace akui
{

Button::Button(s32 x, s32 y, u32 w, u32 h, Window *parent, const std::string &text)
    : Window(parent, text),
      _renderDesc(NULL)
{
	_captured = false;
	_state = up;
	_size = Size(w, h);
	_position = Point(x, y);
	_textColor = uiSettings().buttonTextColor; //RGB15(0,0,0) | BIT(15);
	_style = single;
	_alignment = center;
}

Button::~Button()
{
	if (_renderDesc)
		delete _renderDesc;
}

void Button::draw()
{
	if (NULL == _renderDesc)
		_renderDesc = new ButtonDesc();
	const Point topLeft = position();
	const Point bottomRight = position() + size();

	Rect rect(topLeft, bottomRight);
	_renderDesc->draw(rect, selectedEngine());
}

Window &Button::loadAppearance(const std::string &aFileName)
{
	_renderDesc = new ButtonDesc();
	_renderDesc->setButton(this);
	_renderDesc->loadData(aFileName);

	return *this;
}

bool Button::process(const Message &msg)
{
	return isVisible && (msg.id() > Message::touchMessageStart && msg.id() < Message::touchMessageEnd) ? processTouchMessage((TouchMessage &)msg) : false;
}

bool Button::processTouchMessage(const TouchMessage &msg)
{
	if (msg.id() != Message::touchUp && msg.id() != Message::touchDown)
		return false;

	Rect myRect(_position.x, _position.y, _position.x + _size.x, _position.y + _size.y);
	bool returnValue = false;

	switch (msg.id()) {
		case (Message::touchUp):
			if (_captured) {
				if (myRect.surrounds(msg.position())) {
					onClicked();
					clicked();
				} else
					onReleased();

				_captured = false;
				returnValue = true;
			}
			_state = up;
			break;
		case (Message::touchDown):
			if (myRect.surrounds(msg.position())) {
				onPressed();
				pressed();
				_captured = true;
				_state = down;
				returnValue = true;
			}
			break;
	}

	return returnValue;
}

void Button::onPressed()
{
}

void Button::onReleased()
{
}

void Button::onClicked()
{
}

///////////////////////////////// desc ////////////////
ButtonDesc::ButtonDesc()
{
	_button = NULL;
	_textColor = RGB15(31, 31, 31);
}

ButtonDesc::~ButtonDesc()
{
	//if( NULL != _background )
	//	destroyBMP15( _background );
}

void ButtonDesc::draw(const Rect &area, GRAPHICS_ENGINE engine) const
{
	const u32 *pBuffer = NULL;
	u32 height = 0;
	if (_background.valid()) {
		pBuffer = _background.buffer();
		height = _background.height();
		if (_button->style() != Button::single)
		{
			height /= 2;
			if (Button::down == _button->state())
				pBuffer += _background.width() * _background.height() / 4;
		}
	}

	if (NULL != pBuffer)
	{
		gdi().maskBlt(pBuffer, area.position().x, area.position().y,
					  _background.width(), height, _button->selectedEngine());
	}

	// �����������
	u32 textPixels = font().getStringScreenWidth(_button->text().c_str(), _button->text().size());
	u32 textX = 0, textY = area.position().y + ((area.size().y - SYSTEM_FONT_HEIGHT) >> 1) + 1;
	switch (_button->alignment())
	{
	case Button::center:
		textX = area.position().x + ((area.size().x - textPixels) >> 1);
		break;
	case Button::right:
		textX = area.position().x + (area.size().x - textPixels - 4);
		break;
	default:
		textX = area.position().x + 4;
		break;
	}

	if (Button::down == _button->state())
	{
		textX++;
		textY++;
	}
	gdi().setPenColor(_button->textColor(), _button->selectedEngine());
	gdi().textOutRect(textX, textY, area.size().x, area.size().y,
					  _button->text().c_str(), _button->selectedEngine());
}

void ButtonDesc::loadData(const std::string &filename)
{
	// int width = 40;
	int height = 16;
	if (_button)
	{
		// width = _button->size().x;
		height = _button->size().y;
	}

	if (!_background.valid())
	{
		_background = createBMP15FromFile(filename);
		if (_background.valid())
		{
			if (_button->style() == Button::single)
				height = _background.height();
			else
				height = _background.height() / 2;
			_button->setSize(Size(_background.width(), height));
		}
	}
}

} // namespace akui
