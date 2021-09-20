#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define STB_SPRINTF_IMPLEMENTATION
#include <stb/stb_sprintf.h>

const char shadersPath[] = "assets/shaders";

void compileShader (const char* relativePath) {
  char command[256];
  stbsp_sprintf (
    command,
    "glslc -c %s/%s -o %s/%s.spv",
    shadersPath,
    relativePath,
    shadersPath,
    relativePath
  );

  system (command);
}

int main () {
  DIR* shaders = opendir (shadersPath);

  struct dirent* currentEntry = NULL;
  while ((currentEntry = readdir (shaders))) {
    // Make sure that current entry is not a directory
    if (currentEntry->d_type != DT_DIR) {
      // Skip spir-v binaries
      const char* extension = strstr (currentEntry->d_name, "spv");
      if (!extension) {
        compileShader (currentEntry->d_name);
      }
    }
  }

  closedir (shaders);
}
