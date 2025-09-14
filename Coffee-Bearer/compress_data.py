# compress_data.py
import os
import gzip

# Target web assets directory inside /data
web_dir = os.path.join("data", "web")

print("--- Compressing Web Files ---")

for root, _, files in os.walk(web_dir):
    for fname in files:
        if fname.endswith((".css", ".js", ".html")):
            original = os.path.join(root, fname)
            gzipped = original + ".gz"

            # Only regenerate if source is newer
            if not os.path.exists(gzipped) or os.path.getmtime(original) > os.path.getmtime(gzipped):
                print(f"Gzipping {original} -> {gzipped}")
                with open(original, "rb") as fin, gzip.open(gzipped, "wb", compresslevel=9) as fout:
                    fout.writelines(fin)

print("--- Compression Complete ---")
