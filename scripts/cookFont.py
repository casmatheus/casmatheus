import json
import struct
import sys
import os
import subprocess
import shutil
import glob

# --- CONFIGURATION ---
# Paths are relative to the project root (where you run this script from)
RAW_DIR     = os.path.join("assets", "raw", "Fonts")
COOKED_DIR  = os.path.join("assets", "cooked", "Fonts")

BAT_FILE    = "gen.bat"

# These must match the filenames your gen.bat produces!
GEN_JSON    = "outfit_atlas.json" 
GEN_IMG     = "outfit_atlas.png"

def processFont(name):
  rawDir = os.path.join(RAW_DIR, name)
  cookedDir = os.path.join(COOKED_DIR, name)
  batPath = os.path.join(rawDir, BAT_FILE)

  print(f"[{name}] Processing...")

  if not os.path.exists(batPath):
    print(f"   -> Skipping (No {BAT_FILE} found)")
    return

  try:
    subprocess.check_call(BAT_FILE, cwd=rawDir, shell=True)
  except subprocess.CalledProcessError:
    print(f"   -> Error: {BAT_FILE} failed.")
    return

  jsonFiles = glob.glob(os.path.join(rawDir, "*.json"))
  if not jsonFiles:
    print("   -> Error: No JSON file generated.")
    return

  json_path = max(jsonFiles, key=os.path.getmtime)
  pngFiles = glob.glob(os.path.join(rawDir, "*.png"))
  if not pngFiles:
    print("   -> Error: No PNG file generated.")
    return
  imgPath = max(pngFiles, key=os.path.getmtime)

  with open(json_path, 'r') as f:
      data = json.load(f)

  atlas = data["atlas"]
  metrics = data["metrics"]
  glyphs = data["glyphs"]
  kerning = data.get("kerning", [])

  # --- HEADER (48 bytes) ---
  # Magic (4), TexW (4), TexH (4), LineHeight (4), Ascender (4), Descender (4),
  # UnderlineY (4), UnderlineThick (4), GlyphCount (4), KerningCount (4), 
  # PxRange (4), Padding (4)
  magic = 0x464F4E54 # "FONT"
  tex_w = atlas["width"]
  tex_h = atlas["height"]
  px_range = atlas.get("distanceRange", 4.0)

  header = struct.pack(
    '<3I5f2IfI',
    magic, 
    tex_w, tex_h,
    metrics["lineHeight"], metrics["ascender"], metrics["descender"],
    metrics["underlineY"], metrics["underlineThickness"],
    len(glyphs), len(kerning),
    float(px_range), 0 
  )

  # ---  GLYPHS (40 bytes per glyph) ---
  # Unicode (4), Advance (4), PlaneBounds[4] (16), AtlasBounds[4] (16)
  glyphBytes = bytearray()
  for g in glyphs:
    unicode = g["unicode"]
    advance = g["advance"]
    
    pb = g.get("planeBounds", {})
    pl, pb_b, pr, pt = pb.get("left", 0), pb.get("bottom", 0), pb.get("right", 0), pb.get("top", 0)
    ab = g.get("atlasBounds", {})
    al, ab_b, ar, at = ab.get("left", 0), ab.get("bottom", 0), ab.get("right", 0), ab.get("top", 0)

    glyphBytes.extend(struct.pack('<If4f4f', unicode, advance, pl, pb_b, pr, pt, al, ab_b, ar, at))

  # --- KERNING (12 bytes per pair) ---
  # Unicode1 (4), Unicode2 (4), Advance (4)
  kerningBytes = bytearray()
  for k in kerning:
    kerningBytes.extend(struct.pack('<IIf', k["unicode1"], k["unicode2"], k["advance"]))


  if not os.path.exists(cookedDir):
    os.makedirs(cookedDir)

  outBin = os.path.join(cookedDir, "metrics.bin")
  with open(outBin, 'wb') as f:
    f.write(header)
    f.write(glyphBytes)
    f.write(kerningBytes)

  dstImg = os.path.join(cookedDir, "atlas.png")
  if os.path.exists(dstImg): os.remove(dstImg)
  shutil.copy2(imgPath, dstImg)

def main():
  if not os.path.exists(RAW_DIR):
    print(f"Error: Raw font directory '{RAW_DIR}' not found.")
    sys.exit(1)

  items = os.listdir(RAW_DIR)
  folders = [f for f in items if os.path.isdir(os.path.join(RAW_DIR, f))]

  if not folders:
    print("No font folders found.")
    return

  for font in folders:
    processFont(font)

if __name__ == "__main__":
  main()
