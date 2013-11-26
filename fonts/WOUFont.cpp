/*
 *  WOUFont.cpp
 *  Nuvie
 *
 *  Created by Eric Fry on Mon Nov 25 2013.
 *  Copyright (c) 2013. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include <stdio.h>

#include <string>
#include <cctype>

#include "nuvieDefs.h"

#include "Configuration.h"

#include "Screen.h"
#include "U6Lzw.h"
#include "U6Shape.h"

#include "WOUFont.h"
#include "Game.h"

WOUFont::WOUFont()
{
 font_data = NULL;
 num_chars = 0;
 offset = 0;
 height = 0;
 pixel_char = 0;
 default_color = FONT_COLOR_U6_NORMAL;
 default_highlight_color = FONT_COLOR_U6_HIGHLIGHT;
}

WOUFont::~WOUFont()
{
 if(font_data != NULL)
   free(font_data);
}

bool WOUFont::init(const char *filename)
{

	U6Lzw lzw;
	uint32 decomp_size;
	font_data = lzw.decompress_file(filename, decomp_size);

	height = font_data[0];
	pixel_char = font_data[2];

 num_chars = 256;
	if(Game::get_game()->get_game_type() != NUVIE_GAME_U6) {
		default_color = FONT_COLOR_WOU_NORMAL;
		default_highlight_color = FONT_COLOR_WOU_HIGHLIGHT;
	}
 return true;
}

bool WOUFont::initWithBuffer(unsigned char *buffer, uint32 buffer_len)
{
	font_data = buffer;

	height = font_data[0];
	pixel_char = font_data[2];

 num_chars = 256;
	if(Game::get_game()->get_game_type() != NUVIE_GAME_U6) {
		default_color = FONT_COLOR_WOU_NORMAL;
		default_highlight_color = FONT_COLOR_WOU_HIGHLIGHT;
	}
 return true;
}

uint16 WOUFont::getCharWidth(uint8 c)
{
	if(font_data == NULL)
		return 0;

	return font_data[0x4 + get_char_num(c)];
}


uint16 WOUFont::drawChar(Screen *screen, uint8 char_num, uint16 x, uint16 y,
                    uint8 color)
{
	unsigned char buf[121]; // 11x11
	unsigned char *pixels;
	uint16 width;

	if(font_data == NULL)
		return false;

	memset(buf,0xff,121);

	pixels = font_data + font_data[0x204 + char_num] * 256 + font_data[0x104 + char_num];
	width = font_data[0x4 + char_num];

	//pixels += y * pitch + x;
	for(uint8 i=0;i<(width*height);i++)
	{
		if(pixels[i] == pixel_char)
			buf[i] = color;
	}

	screen->blit(x,y,buf,8,width,height,width,true,NULL);
	return width;
}

uint16 WOUFont::drawStringToShape(U6Shape *shp, const char *str, uint16 x, uint16 y, uint8 color)
{
	uint16 i;
	uint16 string_len = strlen(str);

	if(font_data == NULL)
		return x;

	for(i=0;i<string_len;i++)
	{
		x += drawCharToShape(shp, get_char_num(str[i]), x, y, color);
	}

	return x;
}

uint8 WOUFont::drawCharToShape(U6Shape *shp, uint8 char_num, uint16 x, uint16 y,
                    uint8 color)
{
	unsigned char *pixels;
	uint16 i,j;
	unsigned char *font;
	uint16 pitch;
	uint16 dst_w, dst_h;

	pixels = shp->get_data();
	shp->get_size(&dst_w, &dst_h);
	pitch = dst_w;

	pixels += y * pitch + x;

	uint16 width;

	font = font_data + font_data[0x204 + char_num] * 256 + font_data[0x104 + char_num];
	width = font_data[0x4 + char_num];

	for(i=0;i<height;i++)
	{
		for(j=0;j<width;j++)
		{
			if(font[j]==pixel_char)
			{
				pixels[j] = color;
			}
		}

		font += width;
		pixels += pitch;
	}

	return width;
}
