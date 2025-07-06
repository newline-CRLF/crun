
import codecs

file_path = "C:/Users/Owner/Desktop/for_github/crun/crun.rc"

# Read the content as UTF-8 (assuming current content is UTF-8)
with codecs.open(file_path, "r", "utf-8") as f:
    content = f.read()

# Write the content as Shift-JIS
with codecs.open(file_path, "w", "shift_jis") as f:
    f.write(content)

print(f"'{file_path}' has been re-encoded to Shift-JIS.")
