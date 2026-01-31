#!/usr/bin/env python3
import argparse
import os
import struct
import sys
from PIL import Image

def png_to_lmsk(png_path, lmsk_path, target_size=None):
    """
    Convert a PNG image to LMSK binary format
    
    Args:
        png_path: Path to input PNG file
        lmsk_path: Path to output LMSK file
        target_size: Optional tuple (width, height) to resize the image
    """
    try:
        # Open the PNG image
        img = Image.open(png_path)
        
        # Resize if target size is specified
        if target_size is not None:
            target_width, target_height = target_size
            print(f"Resizing image from {img.size[0]}x{img.size[1]} to {target_width}x{target_height}")
            img = img.resize((target_width, target_height))
        
        # Get image dimensions
        width, height = img.size
        
        # Check if image has alpha channel
        # Supported modes: RGBA, LA (grayscale+alpha), PA (palette+alpha),
        # or P mode with transparency info
        has_alpha = (img.mode in ('RGBA', 'LA') or 
                    (img.mode == 'P' and 'transparency' in img.info) or
                    (img.mode == 'PA'))
        
        print(f"Processing image: {png_path}")
        print(f"Dimensions: {width}x{height}, Mode: {img.mode}")
        print(f"Alpha channel: {'Present' if has_alpha else 'Absent'}")
        
        # Construct 16-byte file header
        # chName[5]: "LMSK" + null terminator
        ch_name = b"LMSK\x00"
        
        # Version: u4Major=0, u4Minor=1 -> 0x01
        version = 0x01
        
        # Image dimensions (little-endian int16)
        
        # Flags byte (uint8_t):
        # bit 0-2: u3AlphaMSBCount = 7 (0b111)
        # bit 3:   bRaw = 1 (0b1)  
        # bit 4-7: reserved = 0 (0b0000)
        # Combined: 0000 1 111 = 0x0F
        flags = 0x0F
        
        # chFloorCount = 0
        floor_count = 0
        
        # Reserved 4 bytes (uint32_t)
        reserved = 0
        
        # Pack header (< indicates little-endian)
        # Format: 5s (5-byte string) + B (1 byte) + h (2 bytes) + h (2 bytes) + 
        #         B (1 byte) + B (1 byte) + I (4 bytes) = 16 bytes
        header = struct.pack('<5sBhhBBI', 
                           ch_name,      # 5 bytes
                           version,      # 1 byte  
                           width,        # 2 bytes (int16)
                           height,       # 2 bytes (int16)
                           flags,        # 1 byte
                           floor_count,  # 1 byte
                           reserved)     # 4 bytes
        
        # Verify header size
        assert len(header) == 16, f"Header size error: {len(header)} bytes, expected 16 bytes"
        
        # Process pixel data
        if has_alpha:
            # Has alpha channel: extract alpha values
            if img.mode == 'RGBA':
                # Convert to RGBA and extract alpha channel (4th byte of every 4 bytes)
                img_rgba = img.convert('RGBA')
                pixel_data = img_rgba.tobytes()
                # Alpha is the 4th channel (index 3)
                output_data = pixel_data[3::4]
                
            elif img.mode == 'LA':
                # Grayscale+Alpha mode (2nd byte of every 2 bytes)
                img_la = img.convert('LA')
                pixel_data = img_la.tobytes()
                output_data = pixel_data[1::2]
                
            else:
                # Other modes with alpha, convert to RGBA for uniform processing
                img_rgba = img.convert('RGBA')
                pixel_data = img_rgba.tobytes()
                output_data = pixel_data[3::4]
            
        else:
            # No alpha channel: convert to Gray8 grayscale format
            img_gray = img.convert('L')  # 'L' mode is 8-bit grayscale
            output_data = img_gray.tobytes()
        
        # Write binary file
        with open(lmsk_path, 'wb') as f:
            f.write(header)
            f.write(output_data)
            
        print(f"Successfully generated: {lmsk_path}")
        print(f"File size: {16 + len(output_data)} bytes (header 16 bytes + data {len(output_data)} bytes)")
        
    except FileNotFoundError:
        print(f"Error: File not found '{png_path}'")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description='Convert PNG image to LMSK binary format')
    parser.add_argument('input', help='Input PNG file path')
    parser.add_argument('output', nargs='?', help='Output LMSK file path (optional)')
    parser.add_argument('--dim', nargs=2, type=int, metavar=('W', 'H'),
                       help='Resize image to specified width and height before processing')
    
    args = parser.parse_args()
    
    input_png = args.input
    
    # Determine output filename
    if args.output:
        # Use user-specified output filename
        output_lmsk = args.output
        # Auto-append .lmsk extension if missing
        if not output_lmsk.endswith('.lmsk'):
            output_lmsk += '.lmsk'
    else:
        # Auto-generate output filename from input filename
        # Remove original extension and add .lmsk in the same directory
        base_name = os.path.splitext(input_png)[0]
        output_lmsk = base_name + '.lmsk'
    
    # Prepare target size if --dim is specified
    target_size = None
    if args.dim:
        width, height = args.dim
        if width <= 0 or height <= 0:
            print("Error: Dimensions must be positive integers")
            sys.exit(1)
        target_size = (width, height)
    
    png_to_lmsk(input_png, output_lmsk, target_size)

if __name__ == "__main__":
    main()