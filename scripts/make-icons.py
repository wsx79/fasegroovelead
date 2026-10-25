#!/usr/bin/env python3
"""Render the product SVG for application and Windows installer assets.
Requires cairosvg and Pillow; generated assets are committed, no build dependency.
"""
from pathlib import Path
from io import BytesIO
import cairosvg
from PIL import Image
root = Path(__file__).resolve().parents[1]
svg = root / 'Resources/fgl_icon.svg'
def icon(size):
    return Image.open(BytesIO(cairosvg.svg2png(url=str(svg), output_width=size, output_height=size))).convert('RGBA')
icon(512).save(root / 'Resources/icon_512.png')
folder = root / 'installer/windows'
icon(256).save(folder / 'FaseGrooveLead.ico', sizes=[(s,s) for s in [16,24,32,48,64,128,256]])
for factor, suffix in [(1,''),(2,'-2x')]:
    canvas = Image.new('RGB', (164*factor,314*factor), (13,15,18))
    logo = icon(120*factor)
    canvas.paste(logo, (22*factor,64*factor), logo)
    canvas.save(folder / ('wizard-large'+suffix+'.bmp'))
    canvas = Image.new('RGB', (55*factor,55*factor), (13,15,18))
    logo = icon(47*factor)
    canvas.paste(logo, (4*factor,4*factor), logo)
    canvas.save(folder / ('wizard-small'+suffix+'.bmp'))
