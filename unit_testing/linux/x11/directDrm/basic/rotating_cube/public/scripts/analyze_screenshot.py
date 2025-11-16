#!/usr/bin/env python3
"""
PPM Screenshot Analyzer for Cube Rendering Debug

This script analyzes PPM screenshots from the cube rendering application
to help debug visual artifacts and understand what's being rendered.

Usage:
    python3 analyze_screenshot.py --screenshot 0001.ppm
    python3 analyze_screenshot.py --screenshot 0001.ppm --verbose
    python3 analyze_screenshot.py --screenshot 0001.ppm --output analysis.png
"""

import argparse
import os
import sys
import struct
from typing import Tuple, List, Optional
import numpy as np

try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as patches
    from matplotlib.colors import LinearSegmentedColormap
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Warning: matplotlib not available. Install with: pip install matplotlib")

class PPMAnalyzer:
    def __init__(self, filepath: str):
        self.filepath = filepath
        self.width = 0
        self.height = 0
        self.pixels = None
        self.load_ppm()

    def load_ppm(self):
        """Load PPM file and parse pixel data"""
        with open(self.filepath, 'rb') as f:
            # Read PPM header
            magic = f.readline().decode('ascii').strip()
            if magic != 'P6':
                raise ValueError(f"Not a valid PPM P6 file: {magic}")

            # Skip comments
            line = f.readline().decode('ascii').strip()
            while line.startswith('#'):
                line = f.readline().decode('ascii').strip()

            # Parse dimensions
            self.width, self.height = map(int, line.split())

            # Parse max color value
            max_val = int(f.readline().decode('ascii').strip())
            if max_val != 255:
                print(f"Warning: Max color value is {max_val}, expected 255")

            # Read pixel data
            pixel_data = f.read()
            expected_size = self.width * self.height * 3
            if len(pixel_data) != expected_size:
                raise ValueError(f"Pixel data size mismatch: got {len(pixel_data)}, expected {expected_size}")

            # Convert to numpy array (H x W x 3)
            self.pixels = np.frombuffer(pixel_data, dtype=np.uint8)
            self.pixels = self.pixels.reshape(self.height, self.width, 3)

    def analyze_colors(self) -> dict:
        """Analyze color distribution and patterns"""
        analysis = {}

        # Count total non-black pixels
        black_mask = np.all(self.pixels == [0, 0, 0], axis=2)
        non_black_pixels = np.sum(~black_mask)
        total_pixels = self.width * self.height

        analysis['total_pixels'] = total_pixels
        analysis['black_pixels'] = total_pixels - non_black_pixels
        analysis['colored_pixels'] = non_black_pixels
        analysis['fill_percentage'] = (non_black_pixels / total_pixels) * 100

        if non_black_pixels > 0:
            # Find bounding box of non-black pixels
            non_black_coords = np.where(~black_mask)
            min_y, max_y = non_black_coords[0].min(), non_black_coords[0].max()
            min_x, max_x = non_black_coords[1].min(), non_black_coords[1].max()

            analysis['bounding_box'] = {
                'min_x': int(min_x), 'max_x': int(max_x),
                'min_y': int(min_y), 'max_y': int(max_y),
                'width': int(max_x - min_x + 1),
                'height': int(max_y - min_y + 1)
            }

            # Analyze color channels
            colored_region = self.pixels[~black_mask]
            analysis['colors'] = {
                'red_avg': float(np.mean(colored_region[:, 0])),
                'green_avg': float(np.mean(colored_region[:, 1])),
                'blue_avg': float(np.mean(colored_region[:, 2])),
                'red_max': int(np.max(colored_region[:, 0])),
                'green_max': int(np.max(colored_region[:, 1])),
                'blue_max': int(np.max(colored_region[:, 2])),
            }

            # Check for gradients (color variation)
            red_std = np.std(colored_region[:, 0])
            green_std = np.std(colored_region[:, 1])
            blue_std = np.std(colored_region[:, 2])
            analysis['color_variation'] = {
                'red_std': float(red_std),
                'green_std': float(green_std),
                'blue_std': float(blue_std),
                'has_gradients': max(red_std, green_std, blue_std) > 10
            }

        return analysis

    def detect_shapes(self) -> dict:
        """Try to detect geometric shapes in the image"""
        analysis = {}

        # Convert to grayscale for edge detection
        gray = np.mean(self.pixels, axis=2)

        # Simple edge detection (look for significant color changes)
        edges_v = np.abs(np.diff(gray, axis=1))  # Vertical edges
        edges_h = np.abs(np.diff(gray, axis=0))  # Horizontal edges

        edge_threshold = 20  # Adjust as needed
        strong_edges_v = np.sum(edges_v > edge_threshold)
        strong_edges_h = np.sum(edges_h > edge_threshold)

        analysis['vertical_edges'] = int(strong_edges_v)
        analysis['horizontal_edges'] = int(strong_edges_h)
        analysis['total_edges'] = int(strong_edges_v + strong_edges_h)

        # Check if image looks like a cube projection
        non_black_mask = np.any(self.pixels > 0, axis=2)
        if np.sum(non_black_mask) > 0:
            # Look for rectangular patterns
            rows_with_content = np.any(non_black_mask, axis=1)
            cols_with_content = np.any(non_black_mask, axis=0)

            content_height = np.sum(rows_with_content)
            content_width = np.sum(cols_with_content)

            # Aspect ratio analysis
            if content_height > 0:
                aspect_ratio = content_width / content_height
                analysis['aspect_ratio'] = float(aspect_ratio)

                # Guess shape type based on content
                if 0.8 <= aspect_ratio <= 1.2:
                    analysis['likely_shape'] = 'square/cube_face'
                elif aspect_ratio > 2.0:
                    analysis['likely_shape'] = 'stretched_horizontal'
                elif aspect_ratio < 0.5:
                    analysis['likely_shape'] = 'stretched_vertical'
                else:
                    analysis['likely_shape'] = 'rectangular'

        return analysis

    def print_analysis(self, verbose: bool = False):
        """Print analysis results to console"""
        print(f"\n=== PPM Analysis: {os.path.basename(self.filepath)} ===")
        print(f"Dimensions: {self.width} x {self.height}")

        color_analysis = self.analyze_colors()
        shape_analysis = self.detect_shapes()

        print(f"\n📊 Color Analysis:")
        print(f"  Total pixels: {color_analysis['total_pixels']:,}")
        print(f"  Black pixels: {color_analysis['black_pixels']:,}")
        print(f"  Colored pixels: {color_analysis['colored_pixels']:,}")
        print(f"  Fill percentage: {color_analysis['fill_percentage']:.1f}%")

        if color_analysis['colored_pixels'] > 0:
            bbox = color_analysis['bounding_box']
            print(f"\n📦 Bounding Box:")
            print(f"  Position: ({bbox['min_x']}, {bbox['min_y']}) to ({bbox['max_x']}, {bbox['max_y']})")
            print(f"  Size: {bbox['width']} x {bbox['height']}")

            colors = color_analysis['colors']
            print(f"\n🎨 Colors:")
            print(f"  Average RGB: ({colors['red_avg']:.1f}, {colors['green_avg']:.1f}, {colors['blue_avg']:.1f})")
            print(f"  Max RGB: ({colors['red_max']}, {colors['green_max']}, {colors['blue_max']})")

            variation = color_analysis['color_variation']
            if variation['has_gradients']:
                print(f"  ✅ Has color gradients (variation: R={variation['red_std']:.1f}, G={variation['green_std']:.1f}, B={variation['blue_std']:.1f})")
            else:
                print(f"  ❌ Flat colors (little variation)")

        print(f"\n🔍 Shape Analysis:")
        print(f"  Vertical edges: {shape_analysis['vertical_edges']}")
        print(f"  Horizontal edges: {shape_analysis['horizontal_edges']}")
        print(f"  Total edges: {shape_analysis['total_edges']}")

        if 'aspect_ratio' in shape_analysis:
            print(f"  Aspect ratio: {shape_analysis['aspect_ratio']:.2f}")
            print(f"  Likely shape: {shape_analysis['likely_shape']}")

        # Diagnostic assessment
        print(f"\n🩺 Diagnostic Assessment:")
        if color_analysis['colored_pixels'] == 0:
            print("  ❌ COMPLETELY BLACK - No rendering occurred!")
        elif color_analysis['fill_percentage'] < 1.0:
            print(f"  ⚠️  SPARSE RENDERING - Only {color_analysis['fill_percentage']:.1f}% of screen filled")
        elif shape_analysis.get('aspect_ratio', 1.0) > 2.0:
            print("  ⚠️  HORIZONTAL STRETCHING DETECTED - Possible viewport/clipping issue")
        elif shape_analysis.get('total_edges', 0) < 10:
            print("  ⚠️  FEW EDGES DETECTED - May be blob/mess rather than geometric shape")
        else:
            print("  ✅ REASONABLE RENDERING - Shape detected with good edges")

        if verbose:
            print(f"\n🔧 Verbose Details:")
            if color_analysis['colored_pixels'] > 0:
                print(f"  Color std dev: R={color_analysis['color_variation']['red_std']:.2f}, "
                      f"G={color_analysis['color_variation']['green_std']:.2f}, "
                      f"B={color_analysis['color_variation']['blue_std']:.2f}")

    def save_visualization(self, output_path: str):
        """Save visualization with analysis overlay"""
        if not HAS_MATPLOTLIB:
            print("Cannot save visualization: matplotlib not available")
            return

        fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(12, 10))
        fig.suptitle(f'PPM Analysis: {os.path.basename(self.filepath)}')

        # Original image
        ax1.imshow(self.pixels)
        ax1.set_title('Original Image')
        ax1.axis('off')

        # Add bounding box if there are colored pixels
        color_analysis = self.analyze_colors()
        if color_analysis['colored_pixels'] > 0:
            bbox = color_analysis['bounding_box']
            rect = patches.Rectangle((bbox['min_x'], bbox['min_y']),
                                   bbox['width'], bbox['height'],
                                   linewidth=2, edgecolor='white', facecolor='none')
            ax1.add_patch(rect)

        # Grayscale
        gray = np.mean(self.pixels, axis=2)
        ax2.imshow(gray, cmap='gray')
        ax2.set_title('Grayscale')
        ax2.axis('off')

        # Edge detection
        edges_v = np.abs(np.diff(gray, axis=1))
        edges_h = np.abs(np.diff(gray, axis=0))
        # Pad to original size
        edges_v = np.pad(edges_v, ((0, 0), (0, 1)), mode='constant')
        edges_h = np.pad(edges_h, ((0, 1), (0, 0)), mode='constant')
        edges_combined = edges_v + edges_h

        ax3.imshow(edges_combined, cmap='hot')
        ax3.set_title('Edge Detection')
        ax3.axis('off')

        # Color channel analysis
        if color_analysis['colored_pixels'] > 0:
            colors = color_analysis['colors']
            channels = ['Red', 'Green', 'Blue']
            averages = [colors['red_avg'], colors['green_avg'], colors['blue_avg']]
            maxes = [colors['red_max'], colors['green_max'], colors['blue_max']]

            x = np.arange(len(channels))
            width = 0.35

            ax4.bar(x - width/2, averages, width, label='Average', alpha=0.7)
            ax4.bar(x + width/2, maxes, width, label='Maximum', alpha=0.7)
            ax4.set_xlabel('Color Channel')
            ax4.set_ylabel('Value (0-255)')
            ax4.set_title('Color Channel Analysis')
            ax4.set_xticks(x)
            ax4.set_xticklabels(channels)
            ax4.legend()
            ax4.set_ylim(0, 255)
        else:
            ax4.text(0.5, 0.5, 'No colored pixels found',
                    horizontalalignment='center', verticalalignment='center',
                    transform=ax4.transAxes, fontsize=14)
            ax4.set_title('No Color Data')

        plt.tight_layout()
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        print(f"Visualization saved to: {output_path}")

def main():
    parser = argparse.ArgumentParser(
        description="Analyze PPM screenshots from cube rendering",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 analyze_screenshot.py --screenshot 0001.ppm
  python3 analyze_screenshot.py --screenshot ../screenshots/0001.ppm --verbose
  python3 analyze_screenshot.py --screenshot 0001.ppm --output analysis.png
        """
    )
    parser.add_argument('--screenshot', required=True,
                       help='PPM screenshot file to analyze')
    parser.add_argument('--verbose', action='store_true',
                       help='Show detailed analysis')
    parser.add_argument('--output',
                       help='Save visualization to this file (requires matplotlib)')

    args = parser.parse_args()

    # Resolve screenshot path
    screenshot_path = args.screenshot
    if not os.path.exists(screenshot_path):
        # Try relative to screenshots directory
        alt_path = os.path.join('../screenshots', args.screenshot)
        if os.path.exists(alt_path):
            screenshot_path = alt_path
        else:
            print(f"Error: Screenshot file not found: {args.screenshot}")
            print(f"Also tried: {alt_path}")
            sys.exit(1)

    try:
        analyzer = PPMAnalyzer(screenshot_path)
        analyzer.print_analysis(verbose=args.verbose)

        if args.output:
            analyzer.save_visualization(args.output)

    except Exception as e:
        print(f"Error analyzing screenshot: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()