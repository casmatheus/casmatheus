import os

sourceFolders   = ["src"]
outputFilename  = "packedSource.txt"
validExtensions = [".cpp", ".h"]
filesToInclude  = ["shell.html", "Makefile", ".gitmodules", "scripts/packSource.py", 
                   "scripts/cookFont.py"]

separator = f"{'='*20}\n"

def addToPack(packFile, newFilePath):
  packFile.write(separator)       
  packFile.write(f"FILE: {newFilePath}\n")
  packFile.write(separator)
 
  with open(newFilePath, "r", encoding="utf-8") as infile:
    packFile.write(infile.read())
    packFile.write("\n")

def packFiles():
  with open(outputFilename, "w", encoding="utf-8") as outfile:
    for folder in sourceFolders:
      for root, _, files in os.walk(folder):
        for file in files:
          ext = os.path.splitext(file)[1].lower()
          if ext in validExtensions:
            filePath = os.path.join(root, file)
            addToPack(outfile, filePath)

    for filePath in filesToInclude:
      addToPack(outfile, filePath)
            
  print(f"All code has been saved to: {outputFilename}")

if __name__ == "__main__":
  packFiles()

