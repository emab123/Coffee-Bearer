# compress_data.py
import os
import gzip
import shutil

SRC_DIR = "web_src"          # editable files
OUT_DIR = os.path.join("data", "web")  # compressed output

def ensure_outdir(path: str) -> None:
    """Ensure the target directory exists."""
    os.makedirs(os.path.dirname(path), exist_ok=True)

def compress_file(src_path: str, out_path: str) -> None:
    # Skip if already up-to-date
    if os.path.exists(out_path) and os.path.getmtime(out_path) >= os.path.getmtime(src_path):
        print(f"✔ Skipping (up-to-date): {src_path}")
        return

    ensure_outdir(out_path)

    # Compress
    with open(src_path, "rb") as f_in, gzip.open(out_path, "wb", compresslevel=9) as f_out:
        shutil.copyfileobj(f_in, f_out)

    print(f"📁 Compressed: {src_path} → {out_path}")


def main():
    print(f"\n\n{'='*15}>> Compressing Web Assets <<{'='*15}\n\n")

    for root, _, files in os.walk(SRC_DIR):
        for file in files:
            if file.endswith((".html", ".css", ".js")):
                src_path = os.path.join(root, file)
                rel_path = os.path.relpath(src_path, SRC_DIR)
                out_path = os.path.join(OUT_DIR, rel_path + ".gz")

                compress_file(src_path, out_path)

    print(f"\n\n{'='*15}>> Compression Complete <<{'='*15}\n\n")


if __name__ == "__main__":
    main()
