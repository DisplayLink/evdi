
def displayid_gen(resolution_x, resolution_y, refresh_rate=60, bits_per_component=8):
  displayid = bytearray(0x03+1)
  # begin section: base
  # begin block0: mandatory base block
  # DisplayID Version 2, Revision 0
  displayid[0x00] = 0x20
  # displayid[1] bytes in section update at the end
  displayid[0x01] = 0x00
  # Set primary use case to "Desktop Productivity Display"
  displayid[0x02] = 0x04
  # Set extension count (essentially section count) to zero
  displayid[0x03] = 0x00
  # end block0

  # begin block1: mandatory Product Identification data
  block_product_id = bytearray(0x1F+1)
  # data block name: "Product Identification data block"
  block_product_id[0x00] = 0x20
  # block revision and other data
  block_product_id[0x01] = 0x00
  # block_product_id[0x02] Number of Payload Bytes in block update at the end of the block
  # block_product_id[0x03:0x06] IEEE-assigned ID Manufacturer/Vendor ID
  block_product_id[0x03] = 0x12
  block_product_id[0x04] = 0x34
  block_product_id[0x05] = 0x56
  # block_product_id[0x06:0x08] Vendor assigned Product ID Code
  block_product_id[0x06] = 0x34
  block_product_id[0x07] = 0x12
  # block_product_id[0x08:0x0C] Vendor assigned Serial Number
  block_product_id[0x08] = 0x78
  block_product_id[0x09] = 0x56
  block_product_id[0x0A] = 0x34
  block_product_id[0x0B] = 0x12
  # Week of Manufacture
  block_product_id[0x0C] = 0x01
  # Year of Manufacture
  block_product_id[0x0D] = 0xff
  product_name_string = "Dummy Monitor".encode('utf-8')
  # Product Name String Length
  block_product_id[0x0E] = len(product_name_string)
  # block_product_id[0x0F:] String
  block_product_id.extend(product_name_string)
  # update block size
  block_product_id[0x02] = len(block_product_id) - 3
  # end block1

  # add block1 to displayid
  displayid.extend(block_product_id)
  # update section size
  displayid[1] += len(block_product_id)

  # begin block2: mandatory Display Parameters data
  block_display_params = bytearray(0x1F+1)
  # data block name: "Display Parameters data block"
  block_display_params[0x00] = 0x21
  # block revision and other data
  block_display_params[0x01] = 0x00
  # Number of Payload Bytes in block update at the end of the block
  block_display_params[0x02] = 0x00
  # Horizontal Size in centimeters (indeterminate size)
  block_display_params[0x03] = 0x00
  block_display_params[0x04] = 0x00
  # Vertical Size in centimeters (indeterminate size)
  block_display_params[0x05] = 0x0
  block_display_params[0x06] = 0x00
  # block_display_params[0x07:0x09] Horizontal Pixel Count
  block_display_params[0x07] = resolution_x & 0xff
  block_display_params[0x08] = (resolution_x >> 8) & 0xff
  # block_display_params[0x09:0x0B] Vertical Pixel Count
  block_display_params[0x09] = resolution_y & 0xff
  block_display_params[0x0A] = (resolution_y >> 8) & 0xff
  # Feature Support Flags
  block_display_params[0x0B] = 0x00

  red_x = 0.64
  red_y = 0.33
  green_x = 0.21
  green_y = 0.71
  blue_x = 0.15
  blue_y = 0.06
  white_x = 0.3127
  white_y = 0.3290

  def pack_chromacity(x: float, y: float) -> bytearray:
    """
    Packs the chromacity values for x and y into a 24-bit integer.

    Arguments:
    x -- the chromacity x value, in the range of 0-1
    y -- the chromacity y value, in the range of 0-1

    Returns:
    A 24-bit integer with the packed chromacity values.
    """

    # 7:0   LSB 8bit x value
    # 11:8  MSB 4bit x value
    # 15:12 LSB 4bit y value
    # 23:16 MSB 8bit y value

    # Ensure x and y are within the allowed range
    if not (0 <= x <= 1) or not (0 <= y <= 1):
        raise ValueError("x and y must be within the range [0.0, 1.0].")

    # Scale x and y to the full range of their bit depths
    x_scaled = int(round(x * 4095))  # 12-bit full range for x
    y_scaled = int(round(y * 4095))  # 12-bit full range for y

    # Extract MSB and LSB for x and y
    x_lsb = x_scaled & 0xFF
    x_msb = (x_scaled >> 8) & 0x0F
    y_lsb = y_scaled & 0x0F
    y_msb = (y_scaled >> 4) & 0xFF

    # Pack the values into a 24-bit integer
    packed_value = (y_msb << 16) | (y_lsb << 12) | (x_msb << 8) | x_lsb

    # Convert the 24-bit integer to a bytearray of 3 bytes
    return bytearray(packed_value.to_bytes(3))


  # block_display_params[0x0C:0x0F] Native Color Chromaticity Coordinates (RED)
  block_display_params[0x0C:0x0F] = pack_chromacity(red_x, red_y)
  # block_display_params[0x0F:0x12] Native Color Chromaticity Coordinates (GREEN)
  block_display_params[0x0F:0x12] = pack_chromacity(green_x, green_y)
  # block_display_params[0x12:0x15] Native Color Chromaticity Coordinates (BLUE)
  block_display_params[0x12:0x15] = pack_chromacity(blue_x, blue_y)
  # block_display_params[0x15:0x18] Native Color Chromaticity Coordinates (WHITE)
  block_display_params[0x15:0x18] = pack_chromacity(white_x, white_y)

  no_luminance = bytearray(0x8000.to_bytes(2, 'big'))
  # block_display_params[0x18:0x1A] Native Maximum Luminance (Full Coverage)
  block_display_params[0x18:0x1A] = no_luminance
  # block_display_params[0x1A:0x1C] Native Maximum Luminance (10% Rectangular Coverage)
  block_display_params[0x1A:0x1C] = no_luminance
  # block_display_params[0x1C:0x1E] Native Minimum Luminance (Black Level)
  block_display_params[0x1C:0x1E] = no_luminance

  # 000b = Not defined. Source device shall apply display
  # interface-specific rules.
  # 001b = 6bpc.
  # 010b = 8bpc.
  # 011b = 10bpc.
  # 100b = 12bpc.
  # 101b = 16bpc.

  def convert_bpc_to_bits(bpc: int) -> int:
    """
    Converts the given bits per component value to the corresponding
    DisplayID bits per component value.

    Arguments:
    bpc -- the bits per component value

    Returns:
    The DisplayID bits per component value.
    """

    if bpc == 6:
        return 0b000
    elif bpc == 8:
        return 0b001
    elif bpc == 10:
        return 0b010
    elif bpc == 12:
        return 0b011
    elif bpc == 16:
        return 0b100
    else:
        raise ValueError("Invalid bits per component value: " + str(bpc))

  # Native Color Depth and Display Device Technology
  block_display_params[0x1E] = convert_bpc_to_bits(bits_per_component)

  # Native Gamma EOTF (no gamma information given)
  block_display_params[0x1F] = 0xFF
  # end block2

  # add block2 to displayid
  displayid.extend(block_display_params)
  # update section size
  displayid[1] += len(block_display_params)

  return displayid
