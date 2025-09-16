# compress_data.py
import os
import gzip
import shutil
from pathlib import Path

# This line is crucial - it imports PlatformIO's build environment
Import("env") # type: ignore

def compress_web_assets(source, target, env):
    """
    Compresses web assets from web_src to the data directory before
    the SPIFFS image is created. This function is called by PlatformIO.
    """
    print("\n" + "="*15 + ">> Compressing Web Assets <<" + "="*15 + "\n")

    # --- Configuration ---
    # CORRECTED: Use $PROJECT_DIR to point to the root of your project folder
    src_dir = Path(env.subst("$PROJECT_DIR")) / "web_src"
    
    # Target directory for compressed files (inside the project's 'data' folder)
    out_dir = Path(env.subst("$PROJECT_DATA_DIR")) / "web"
    
    # File extensions to compress
    compressible_exts = [".html", ".css", ".js"]
    # File extensions to simply copy
    copy_exts = [".ico"]

    if not src_dir.exists():
        print(f"Warning: Source directory '{src_dir}' not found. Skipping compression.")
        return

    # --- Processing Logic ---
    for src_path in src_dir.rglob("*"):
        if not src_path.is_file():
            continue

        rel_path = src_path.relative_to(src_dir)
        out_path = out_dir / rel_path

        # Ensure the target directory exists
        out_path.parent.mkdir(parents=True, exist_ok=True)

        # Skip if the destination file is newer than the source
        if out_path.exists() and out_path.stat().st_mtime >= src_path.stat().st_mtime:
            print(f"✔ Skipping (up-to-date): {rel_path}")
            continue

        # Decide whether to compress or copy
        if src_path.suffix in compressible_exts:
            # Compress the file
            gz_out_path = out_path.with_suffix(src_path.suffix + ".gz")
            with open(src_path, "rb") as f_in, gzip.open(gz_out_path, "wb", compresslevel=9) as f_out:
                shutil.copyfileobj(f_in, f_out)
            print(f"📁 Compressed: {rel_path} → {gz_out_path.relative_to(out_dir)}")

        elif src_path.suffix in copy_exts:
            # Copy the file
            shutil.copy2(src_path, out_path)
            print(f"📄 Copied: {rel_path} → {out_path.relative_to(out_dir)}")

    print("\n" + "="*15 + ">> Asset Processing Complete <<" + "="*15 + "\n")

# --- PlatformIO Integration ---
# This hook ties our function to the filesystem build process.
env.AddPreAction("$BUILD_DIR/spiffs.bin", compress_web_assets) # type: ignore