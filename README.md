# Nafuda

Nafuda is a lightweight tool for copying project structure and providing code context for LLMs.

Built with C++ and Qt 6, Nafuda provides a fast native experience for extracting project structure and preparing comprehensive code context for LLMs.

## Features

- **High-Performance Native Application**  
  Implemented in C++ with Qt 6 for fast startup and responsive handling of large directory trees.

- **Configurable Context Output**  
  Adjustable modes for different workflows:
  - **Directory Tree:** Visualizes the folder structure (ASCII tree style).
  - **File Content Only:** Copies the code of selected files.
  - **Full Context:** Combines directory structure and file contents in a single paste.

- **Interactive File Selection**  
  Recursive file explorer with **Select All** / **Deselect All** options for precise control over which paths are included.
  
- **Customizable Templates**  
  Define your own formatting style to make the output easier for language models to read and process.

- **File Preview**  
  Integrated file viewer with metadata (file size, format) to validate output before copying.

## Supported Codebases

Nafuda works with any text-based codebase, including:

- Web projects (React, Vue, Node.js)
- Systems and native projects (C++, Rust, Go)
- Scripts (Python, Bash, Lua)
- Configuration files (JSON, YAML, XML)

## Use Cases

- Preparing comprehensive prompts for ChatGPT, Claude, or Gemini
- Sharing project architecture during code reviews or team discussions  
- Isolating specific parts of a large project for documentation
- Helping LLMs understand how files work together to fix bugs

## Build

```
rm -rf build
mkdir build && cd build

cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

cpack -G DEB
```
