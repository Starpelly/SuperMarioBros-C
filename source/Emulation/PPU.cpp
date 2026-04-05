#include "../SMB/SMBEngine.hpp"
#include "../Util/Video.hpp"

#include "PPU.hpp"

static const uint8_t nametableMirrorLookup[][4] = {
	{0, 0, 1, 1}, // Vertical
	{0, 1, 0, 1}  // Horizontal
};

#define RGB(r, g, b) ((uint32_t)(r) << 16 | (uint32_t)(g) << 8 | (uint32_t)(b))

/**
 * Default hardcoded palette.
 */
static constexpr const uint32_t defaultPaletteRGB[64] = {
	RGB(84, 84, 84),
	RGB(0, 30, 116),
	RGB(8, 16, 144),
	RGB(48, 0, 136),
	RGB(68, 0, 100),
	RGB(92, 0, 48),
	RGB(84, 4, 0),
	RGB(60, 24, 0),
	RGB(32, 42, 0),
	RGB(8, 58, 0),
	RGB(0, 64, 0),
	RGB(0, 60, 0),
	RGB(0, 50, 60),
	RGB(0, 0, 0),
	RGB(0, 0, 0),
	RGB(0, 0, 0),
	RGB(152, 150, 152),
	RGB(8, 76, 196),
	RGB(48, 50, 236),
	RGB(92, 30, 228),
	RGB(136, 20, 176),
	RGB(160, 20, 100),
	RGB(152, 34, 32),
	RGB(120, 60, 0),
	RGB(84, 90, 0),
	RGB(40, 114, 0),
	RGB(8, 124, 0),
	RGB(0, 118, 40),
	RGB(0, 102, 120),
	RGB(0, 0, 0),
	RGB(0, 0, 0),
	RGB(0, 0, 0),
	RGB(236, 238, 236),
	RGB(76, 154, 236),
	RGB(120, 124, 236),
	RGB(176, 98, 236),
	RGB(228, 84, 236),
	RGB(236, 88, 180),
	RGB(236, 106, 100),
	RGB(212, 136, 32),
	RGB(160, 170, 0),
	RGB(116, 196, 0),
	RGB(76, 208, 32),
	RGB(56, 204, 108),
	RGB(56, 180, 204),
	RGB(60, 60, 60),
	RGB(0, 0, 0),
	RGB(0, 0, 0),
	RGB(236, 238, 236),
	RGB(168, 204, 236),
	RGB(188, 188, 236),
	RGB(212, 178, 236),
	RGB(236, 174, 236),
	RGB(236, 174, 212),
	RGB(236, 180, 176),
	RGB(228, 196, 144),
	RGB(204, 210, 120),
	RGB(180, 222, 120),
	RGB(168, 226, 144),
	RGB(152, 226, 180),
	RGB(160, 214, 228),
	RGB(160, 162, 160),
	RGB(0, 0, 0),
	RGB(0, 0, 0),
};

/**
 * RGB representation of the NES palette.
 */
const uint32_t* paletteRGB = defaultPaletteRGB;

PPU::PPU(SMBEngine& engine) :
	engine(engine)
{
	currentAddress = 0;
	writeToggle = false;
}

uint8_t PPU::getAttributeTableValue(uint16_t nametableAddress)
{
	nametableAddress = getNametableIndex(nametableAddress);

	// Determine the 32x32 attribute table address
	int row = ((nametableAddress & 0x3e0) >> 5) / 4;
	int col = (nametableAddress & 0x1f) / 4;

	// Determine the 16x16 metatile for the 8x8 tile addressed
	int shift = ((nametableAddress & (1 << 6)) ? 4 : 0) + ((nametableAddress & (1 << 1)) ? 2 : 0);

	// Determine the offset into the attribute table
	int offset = (nametableAddress & 0xc00) + 0x400 - 64 + (row * 8 + col);

	// Determine the attribute table value
	return (nametable[offset] & (0x3 << shift)) >> shift;
}

uint16_t PPU::getNametableIndex(uint16_t address)
{
	address = (address - 0x2000) % 0x1000;
	int table = address / 0x400;
	int offset = address % 0x400;
	int mode = 1; // Mirroring mode for Super Mario Bros.
	return (nametableMirrorLookup[mode][table] * 0x400 + offset) % 2048;
}

uint8_t PPU::readByte(uint16_t address)
{
	// Mirror all addresses above $3fff
	address &= 0x3fff;

	if (address < 0x2000)
	{
		// CHR
		return engine.getCHR()[address];
	}
	else if (address < 0x3f00)
	{
		// Nametable
		return nametable[getNametableIndex(address)];
	}

	return 0;
}

uint8_t PPU::readCHR(int index)
{
	if (index < 0x2000)
	{
		return engine.getCHR()[index];
	}
	else
	{
		return 0;
	}
}

uint8_t PPU::readDataRegister()
{
	uint8_t value = vramBuffer;
	vramBuffer = readByte(currentAddress);

	if (!(ppuCtrl & (1 << 2)))
	{
		currentAddress++;
	}
	else
	{
		currentAddress += 32;
	}

	return value;
}

uint8_t PPU::readRegister(uint16_t address)
{
	static int cycle = 0;
	switch(address)
	{
	// PPUSTATUS
	case 0x2002:
		writeToggle = false;
		return (cycle++ % 2 == 0 ? 0xc0 : 0);
	// OAMDATA
	case 0x2004:
		return oam[oamAddress];
	// PPUDATA
	case 0x2007:
		return readDataRegister();
	default:
		break;
	}

	return 0;
}

void PPU::renderTile(uint32_t* buffer, int index, int xOffset, int yOffset)
{
	// Lookup the pattern table entry
	uint16_t tile = readByte(index) + (ppuCtrl & (1 << 4) ? 256 : 0);
	uint8_t attribute = getAttributeTableValue(index);

	// Read the pixels of the tile
	for( int row = 0; row < 8; row++ )
	{
		uint8_t plane1 = readCHR(tile * 16 + row);
		uint8_t plane2 = readCHR(tile * 16 + row + 8);

		for( int column = 0; column < 8; column++ )
		{
			uint8_t paletteIndex = (((plane1 & (1 << column)) ? 1 : 0) + ((plane2 & (1 << column)) ? 2 : 0));
			uint8_t colorIndex = palette[attribute * 4 + paletteIndex];
			if( paletteIndex == 0 )
			{
				// skip transparent pixels
				//colorIndex = palette[0];
				continue;
			}
			uint32_t pixel = 0xff000000 | paletteRGB[colorIndex];

			int x = (xOffset + (7 - column));
			int y = (yOffset + row);
			if (x < 0 || x >= 256 || y < 0 || y >= 240)
			{
				continue;
			}
			buffer[y * 256 + x] = pixel;
		}
	}

}

void PPU::render(uint32_t* buffer)
{
	// Clear the buffer with the background color
	for (int index = 0; index < 256 * 240; index++)
	{
		buffer[index] = paletteRGB[palette[0]];
	}

	// Draw sprites behind the backround
	if (ppuMask & (1 << 4)) // Are sprites enabled?
	{
		// Sprites with the lowest index in OAM take priority.
		// Therefore, render the array of sprites in reverse order.
		//
		for (int i = 63; i >= 0; i--)
		{
			// Read OAM for the sprite
			uint8_t y          = oam[i * 4];
			uint8_t index      = oam[i * 4 + 1];
			uint8_t attributes = oam[i * 4 + 2];
			uint8_t x          = oam[i * 4 + 3];

			// Check if the sprite has the correct priority
			if (!(attributes & (1 << 5)))
			{
				continue;
			}

			// Check if the sprite is visible
			if( y >= 0xef || x >= 0xf9 )
			{
				continue;
			}

			// Increment y by one since sprite data is delayed by one scanline
			//
			y++;

			// Determine the tile to use
			uint16_t tile = index + (ppuCtrl & (1 << 3) ? 256 : 0);
			bool flipX = attributes & (1 << 6);
			bool flipY = attributes & (1 << 7);

			// Copy pixels to the framebuffer
			for( int row = 0; row < 8; row++ )
			{
				uint8_t plane1 = readCHR(tile * 16 + row);
				uint8_t plane2 = readCHR(tile * 16 + row + 8);

				for( int column = 0; column < 8; column++ )
				{
					uint8_t paletteIndex = (((plane1 & (1 << column)) ? 1 : 0) + ((plane2 & (1 << column)) ? 2 : 0));
					uint8_t colorIndex = palette[0x10 + (attributes & 0x03) * 4 + paletteIndex];
					if( paletteIndex == 0 )
					{
						// Skip transparent pixels
						continue;
					}
					uint32_t pixel = 0xff000000 | paletteRGB[colorIndex];

					int xOffset = 7 - column;
					if( flipX )
					{
						xOffset = column;
					}
					int yOffset = row;
					if( flipY )
					{
						yOffset = 7 - row;
					}

					int xPixel = (int)x + xOffset;
					int yPixel = (int)y + yOffset;
					if (xPixel < 0 || xPixel >= 256 || yPixel < 0 || yPixel >= 240)
					{
						continue;
					}

					buffer[yPixel * 256 + xPixel] = pixel;
				}
			}
		}
	}

	// Draw the background (nametable)
	if (ppuMask & (1 << 3)) // Is the background enabled?
	{
		int scrollX = (int)ppuScrollX + ((ppuCtrl & (1 << 0)) ? 256 : 0);
		int xMin = scrollX / 8;
		int xMax = ((int)scrollX + 256) / 8;
		for (int x = 0; x < 32; x++)
		{
			for (int y = 0; y < 4; y++)
			{
				// Render the status bar in the same position (it doesn't scroll)
				renderTile(buffer, 0x2000 + 32 * y + x, x * 8, y * 8);
			}
		}
		for (int x = xMin; x <= xMax; x++)
		{
			for (int y = 4; y < 30; y++)
			{
				// Determine the index of the tile to render
				int index;
				if (x < 32)
				{
					index = 0x2000 + 32 * y + x;
				}
				else if (x < 64)
				{
					index = 0x2400 + 32 * y + (x - 32);
				}
				else
				{
					index = 0x2800 + 32 * y + (x - 64);
				}

				// Render the tile
				renderTile(buffer, index, (x * 8) - (int)scrollX, (y * 8));
			}
		}
	}

	// Draw sprites in front of the background
	if (ppuMask & (1 << 4))
	{
		// Sprites with the lowest index in OAM take priority.
		// Therefore, render the array of sprites in reverse order.
		//
		// We render sprite 0 first as a special case (coin indicator).
		//
		for (int j = 64; j > 0; j--)
		{
			// Start at 0, then 63, 62, 61, ..., 1
			//
			int i = j % 64;

			// Read OAM for the sprite
			uint8_t y          = oam[i * 4];
			uint8_t index      = oam[i * 4 + 1];
			uint8_t attributes = oam[i * 4 + 2];
			uint8_t x          = oam[i * 4 + 3];

			// Check if the sprite has the correct priority
			//
			// Special case for sprite 0, tile 0xff in Super Mario Bros.
			// (part of the pixels for the coin indicator)
			//
			if (attributes & (1 << 5) && !(i == 0 && index == 0xff))
			{
				continue;
			}

			// Check if the sprite is visible
			if( y >= 0xef || x >= 0xf9 )
			{
				continue;
			}

			// Increment y by one since sprite data is delayed by one scanline
			//
			y++;

			// Determine the tile to use
			uint16_t tile = index + (ppuCtrl & (1 << 3) ? 256 : 0);
			bool flipX = attributes & (1 << 6);
			bool flipY = attributes & (1 << 7);

			// Copy pixels to the framebuffer
			for( int row = 0; row < 8; row++ )
			{
				uint8_t plane1 = readCHR(tile * 16 + row);
				uint8_t plane2 = readCHR(tile * 16 + row + 8);

				for( int column = 0; column < 8; column++ )
				{
					uint8_t paletteIndex = (((plane1 & (1 << column)) ? 1 : 0) + ((plane2 & (1 << column)) ? 2 : 0));
					uint8_t colorIndex = palette[0x10 + (attributes & 0x03) * 4 + paletteIndex];
					if( paletteIndex == 0 )
					{
						// Skip transparent pixels
						continue;
					}
					uint32_t pixel = 0xff000000 | paletteRGB[colorIndex];

					int xOffset = 7 - column;
					if( flipX )
					{
						xOffset = column;
					}
					int yOffset = row;
					if( flipY )
					{
						yOffset = 7 - row;
					}

					int xPixel = (int)x + xOffset;
					int yPixel = (int)y + yOffset;
					if (xPixel < 0 || xPixel >= 256 || yPixel < 0 || yPixel >= 240)
					{
						continue;
					}

					// Special case for sprite 0, tile 0xff in Super Mario Bros.
					// (part of the pixels for the coin indicator)
					//
					if (i == 0 && index == 0xff && row == 5 && column > 3 && column < 6)
					{
						continue;
					}

					buffer[yPixel * 256 + xPixel] = pixel;
				}
			}
		}
	}
}

void PPU::writeAddressRegister(uint8_t value)
{
	if (!writeToggle)
	{
		// Upper byte
		currentAddress = (currentAddress & 0xff) | (((uint16_t)value << 8) & 0xff00);
	}
	else
	{
		// Lower byte
		currentAddress = (currentAddress & 0xff00) | (uint16_t)value;
	}
	writeToggle = !writeToggle;
}

void PPU::writeByte(uint16_t address, uint8_t value)
{
	// Mirror all addrsses above $3fff
	address &= 0x3fff;

	if (address < 0x2000)
	{
		// CHR (no-op)
	}
	else if (address < 0x3f00)
	{
		nametable[getNametableIndex(address)] = value;
	}
	else if (address < 0x3f20)
	{
		// Palette data
		palette[address - 0x3f00] = value;

		// Mirroring
		if (address == 0x3f10 || address == 0x3f14 || address == 0x3f18 || address == 0x3f1c)
		{
			palette[address - 0x3f10] = value;
		}
	}
}

void PPU::writeDataRegister(uint8_t value)
{
	writeByte(currentAddress, value);
	if (!(ppuCtrl & (1 << 2)))
	{
		currentAddress++;
	}
	else
	{
		currentAddress += 32;
	}
}

void PPU::writeDMA(uint8_t page)
{
	uint16_t address = (uint16_t)page << 8;
	for (int i = 0; i < 256; i++)
	{
		oam[oamAddress] = engine.readData(address);
		address++;
		oamAddress++;
	}
}

void PPU::writeRegister(uint16_t address, uint8_t value)
{
	switch(address)
	{
	// PPUCTRL
	case 0x2000:
		ppuCtrl = value;
		break;
	// PPUMASK
	case 0x2001:
		ppuMask = value;
		break;
	// OAMADDR
	case 0x2003:
		oamAddress = value;
		break;
	// OAMDATA
	case 0x2004:
		oam[oamAddress] = value;
		oamAddress++;
		break;
	// PPUSCROLL
	case 0x2005:
		if (!writeToggle)
		{
			ppuScrollX = value;
		}
		else
		{
			ppuScrollY = value;
		}
		writeToggle = !writeToggle;
		break;
	// PPUADDR
	case 0x2006:
		writeAddressRegister(value);
		break;
	// PPUDATA
	case 0x2007:
		writeDataRegister(value);
		break;
	default:
		break;
	}
}
