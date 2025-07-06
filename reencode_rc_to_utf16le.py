
import codecs

file_path = "C:/Users/Owner/Desktop/for_github/crun/crun.rc"

# Read the content as UTF-8 (assuming current content is UTF-8)
with codecs.open(file_path, "r", "utf-8") as f:
    content = f.read()

# Write the content as UTF-16 LE without BOM
with codecs.open(file_path, "w", "utf-16-le") as f:
    f.write(content)

print(f"'{file_path}' has been re-encoded to UTF-16 LE without BOM.")
