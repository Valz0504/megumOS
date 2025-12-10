#!/usr/bin/env python3
"""
Generate both palette and wallpaper from mascot.png for VGA mode 13h
This ensures the palette contains all the colors needed for the mascot image
"""

from PIL import Image
import sys
import os

# VGA mode 13h dimensions
SCREEN_WIDTH = 320
SCREEN_HEIGHT = 200

# Reserved color indices (will be overridden in graphics.c)
RESERVED_INDICES = {
    248: (0, 0, 0),       # DEFAULT_COLOR_BG - black
    250: (0, 63, 0),      # DEFAULT_COLOR_FG - green
    251: (63, 63, 63),    # COLOR_ASCII_WHITE - white
}

def generate_palette_from_image(img, num_colors=256):
    """Generate optimized palette from image using quantization"""
    # Reserve slots for special colors
    num_colors_for_image = num_colors - len(RESERVED_INDICES)
    
    # Convert to RGB and quantize
    if img.mode == 'RGBA':
        background = Image.new('RGB', img.size, (255, 255, 255))
        background.paste(img, mask=img.split()[3])
        img = background
    elif img.mode != 'RGB':
        img = img.convert('RGB')
    
    # Use PIL's quantization to find best colors
    img_quantized = img.quantize(colors=num_colors_for_image, method=2)
    palette_data = img_quantized.getpalette()
    
    # Convert to list of RGB tuples and scale to VGA range (0-63)
    palette = []
    for i in range(0, num_colors_for_image * 3, 3):
        r = min(63, palette_data[i] >> 2)
        g = min(63, palette_data[i + 1] >> 2)
        b = min(63, palette_data[i + 2] >> 2)
        palette.append((r, g, b))
    
    # Pad to 256 colors if needed
    while len(palette) < num_colors:
        palette.append((0, 0, 0))
    
    # Set reserved indices
    for idx, (r, g, b) in RESERVED_INDICES.items():
        if idx < len(palette):
            palette[idx] = (r, g, b)
    
    return palette

def write_palette_header(palette, output_file):
    """Write palette to C header file"""
    with open(output_file, 'w') as f:
        f.write('#ifndef _PALETTE_GENERATED_H\n')
        f.write('#define _PALETTE_GENERATED_H\n')
        f.write('\n')
        f.write('// Auto-generated palette from mascot.png\n')
        f.write('static const unsigned char mascot_palette[768] = {\n')
        
        for i, (r, g, b) in enumerate(palette):
            if i % 4 == 0:
                f.write('  ')
            f.write(f'{r:>2}, {g:>2}, {b:>2}')
            if i < len(palette) - 1:
                f.write(',')
            if (i + 1) % 4 == 0:
                f.write('\n')
            else:
                f.write(' ')
        
        if len(palette) % 4 != 0:
            f.write('\n')
        
        f.write('};\n')
        f.write('\n')
        f.write('#endif\n')
    
    print(f"Generated palette header: {output_file}")

def find_closest_color(r, g, b, palette):
    """Find closest color in palette using Euclidean distance"""
    # Scale input RGB (0-255) to match palette range (0-63)
    r_scaled = r >> 2
    g_scaled = g >> 2
    b_scaled = b >> 2
    
    min_dist = float('inf')
    closest_idx = 0
    
    for i, (pr, pg, pb) in enumerate(palette):
        dist = (r_scaled - pr)**2 + (g_scaled - pg)**2 + (b_scaled - pb)**2
        if dist < min_dist:
            min_dist = dist
            closest_idx = i
    
    return closest_idx

def write_wallpaper_header(img, palette, output_file):
    """Write wallpaper data to C header file"""
    pixels = img.load()
    
    with open(output_file, 'w') as f:
        f.write('#ifndef _WALLPAPER_H\n')
        f.write('#define _WALLPAPER_H\n')
        f.write('\n')
        f.write('#include <stdint.h>\n')
        f.write('\n')
        f.write(f'const uint8_t wallpaper[{SCREEN_WIDTH * SCREEN_HEIGHT}] = {{\n')
        
        for y in range(SCREEN_HEIGHT):
            row_data = []
            for x in range(SCREEN_WIDTH):
                r, g, b = pixels[x, y]
                palette_idx = find_closest_color(r, g, b, palette)
                row_data.append(str(palette_idx))
            
            # Write 16 values per line
            for i in range(0, len(row_data), 16):
                chunk = row_data[i:i+16]
                f.write(','.join(chunk) + ',\n')
        
        # Remove trailing comma
        f.seek(f.tell() - 2)
        f.write('\n')
        f.write('};\n')
        f.write('\n')
        f.write('#endif\n')
    
    print(f"Generated wallpaper header: {output_file}")

def main():
    input_file = 'mascot.png'
    palette_output = 'src/graphics/palette_generated.h'
    wallpaper_output = 'src/header/graphics/wallpaper.h'
    
    if not os.path.exists(input_file):
        print(f"Error: {input_file} not found")
        return 1
    
    print(f"Loading {input_file}...")
    img = Image.open(input_file)
    print(f"Original size: {img.size}, mode: {img.mode}")
    
    # Generate palette from full resolution image for better color representation
    print("Generating optimized 256-color palette...")
    palette = generate_palette_from_image(img, num_colors=256)
    
    # Write palette header
    write_palette_header(palette, palette_output)
    
    # Resize image for wallpaper
    print(f"Resizing to {SCREEN_WIDTH}x{SCREEN_HEIGHT}...")
    img_resized = img.resize((SCREEN_WIDTH, SCREEN_HEIGHT), Image.Resampling.LANCZOS)
    
    # Convert to RGB if needed
    if img_resized.mode == 'RGBA':
        background = Image.new('RGB', img_resized.size, (255, 255, 255))
        background.paste(img_resized, mask=img_resized.split()[3])
        img_resized = background
    elif img_resized.mode != 'RGB':
        img_resized = img_resized.convert('RGB')
    
    # Write wallpaper header
    write_wallpaper_header(img_resized, palette, wallpaper_output)
    
    print("\nSuccess! Generated files:")
    print(f"  - {palette_output} (256-color VGA palette)")
    print(f"  - {wallpaper_output} ({SCREEN_WIDTH}x{SCREEN_HEIGHT} indexed image)")
    print("\nNow run 'make build' to rebuild the kernel with new graphics!")
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
