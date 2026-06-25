#!/usr/bin/env python3
"""
Combine two UF2 files into one for dual-slot boot.

Preserves family ID blocks and all data blocks with correct metadata.

Usage:
    python3 combine_uf2.py bootloader.uf2 firmware.uf2 combined.uf2
"""

import struct
import sys

UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END    = 0x0AB16F30
FAMILY_ID_ADDR    = 0x10FFFF00
BLOCK_SIZE        = 512

# Flag bits
UF2_FLAG_FAMILY_ID = 0x00002000   # family ID present in block header
UF2_FLAG_METADATA  = 0x00001000   # not main flash data


def is_family_meta(raw):
    """Check if this is a family ID metadata block."""
    if len(raw) != BLOCK_SIZE:
        return False
    magic0, magic1, flags, addr, payload_size, block_no, total_blocks, family = \
        struct.unpack('<IIIIIIII', raw[:32])
    if magic0 != UF2_MAGIC_START0 or magic1 != UF2_MAGIC_START1:
        return False
    return addr == FAMILY_ID_ADDR


def read_uf2_blocks(path):
    """Read all raw 512-byte blocks from a UF2 file."""
    blocks = []
    with open(path, 'rb') as f:
        while True:
            raw = f.read(BLOCK_SIZE)
            if not raw:
                break
            if len(raw) != BLOCK_SIZE:
                continue
            magic0, magic1, flags, addr, payload_size, block_no, total_blocks, family = \
                struct.unpack('<IIIIIIII', raw[:32])
            if magic0 != UF2_MAGIC_START0 or magic1 != UF2_MAGIC_START1:
                continue
            blocks.append(raw)
    return blocks


def write_uf2_combined(path, meta_block, data_blocks):
    """
    Write combined UF2 with correct block numbering.

    meta_block: raw 512-byte family metadata block (or None)
    data_blocks: list of raw 512-byte data blocks
    """
    total = len(data_blocks)
    block_no = 0
    
    with open(path, 'wb') as f:
        # Write metadata block first with block_no=0, total_blocks=total
        if meta_block is not None:
            b = bytearray(meta_block)
            struct.pack_into('<II', b, 20, 0, total)  # block_no at 20, total_blocks at 24
            f.write(b)
        
        # Write data blocks with consecutive numbering
        for i, raw in enumerate(data_blocks):
            b = bytearray(raw)
            struct.pack_into('<II', b, 20, i, total)  # block_no at 20, total_blocks at 24
            f.write(b)


def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <bootloader.uf2> <firmware.uf2> <combined.uf2>")
        sys.exit(1)
    
    bl_path, fw_path, out_path = sys.argv[1], sys.argv[2], sys.argv[3]
    
    # Read all raw blocks from both files
    bl_raw = read_uf2_blocks(bl_path)
    fw_raw = read_uf2_blocks(fw_path)
    
    # Separate metadata blocks (0x10FFFF00) from data blocks
    bl_meta = [b for b in bl_raw if is_family_meta(b)]
    bl_data = [b for b in bl_raw if not is_family_meta(b)]
    fw_meta = [b for b in fw_raw if is_family_meta(b)]
    fw_data = [b for b in fw_raw if not is_family_meta(b)]
    
    print(f"Bootloader: {len(bl_meta)} meta, {len(bl_data)} data blocks")
    print(f"Firmware:   {len(fw_meta)} meta, {len(fw_data)} data blocks")
    
    # Use the first metadata block found (should be identical)
    meta_block = bl_meta[0] if bl_meta else (fw_meta[0] if fw_meta else None)
    
    # Combine data blocks (bootloader first, then firmware)
    all_data = bl_data + fw_data
    
    # Validate no address overlap
    for b in all_data:
        magic0, magic1, flags, addr, payload_size, _, _, _ = \
            struct.unpack('<IIIIIIII', b[:32])
        # Check if we have a family ID mismatch (just informational)
    
    if bl_data and fw_data:
        # Find first and last addresses
        def block_addr(raw):
            return struct.unpack('<I', raw[12:16])[0]
        bl_min = block_addr(bl_data[0])
        bl_max = block_addr(bl_data[-1]) + 256
        fw_min = block_addr(fw_data[0])
        fw_max = block_addr(fw_data[-1]) + 256
        print(f"Bootloader: 0x{bl_min:08X} - 0x{bl_max:08X} ({bl_max - bl_min} bytes)")
        print(f"Firmware:   0x{fw_min:08X} - 0x{fw_max:08X} ({fw_max - fw_min} bytes)")
        if bl_max > fw_min:
            print("ERROR: Overlap!")
            sys.exit(1)
        print(f"Gap: {fw_min - bl_max} bytes ({(fw_min - bl_max)/1024:.1f} KB)")
    
    total_kb = (len(all_data) * 256) / 1024
    print(f"Combined: {len(all_data)} data blocks ({total_kb:.0f} KB payload)")
    
    write_uf2_combined(out_path, meta_block, all_data)
    print(f"Written: {out_path}")
    
    # Verify
    verify = read_uf2_blocks(out_path)
    v_meta = [b for b in verify if is_family_meta(b)]
    v_data = [b for b in verify if not is_family_meta(b)]
    v_total = struct.unpack('<I', verify[0][20:24])[0] if verify else 0
    print(f"Verify: {len(v_meta)} meta, {len(v_data)} data, total_blocks={v_total}")
    
    # Check family ID in first data block
    if verify:
        _, _, flags, _, _, _, _, fam = struct.unpack('<IIIIIIII', verify[0][:32])
        if is_family_meta(verify[0]):
            # meta block is first, check data block
            _, _, _, _, _, _, _, fam = struct.unpack('<IIIIIIII', verify[1][:32])
        print(f"Family ID: 0x{fam:08X}")


if __name__ == '__main__':
    main()
