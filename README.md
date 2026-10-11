# CODEFORGE

CODEFORGE estimates the time and space complexity of a C++ function named `algorithm`. It tokenizes submitted source code, identifies selected loops, recursion patterns, algorithms, and data structures, and returns an estimate through a Crow HTTP API. The estimates are heuristic; they are not a proof of complexity for arbitrary C++ programs.

## Team

A two-person group project.

| Member | GitHub | Role |
|---|---|---|
| Sakiful Siam | [@SakifulSiam](https://github.com/SakifulSiam) | Backend: complexity analysis engine and API |
| Ziaul Hasan Hamim | [@ziaulhasanhamim](https://github.com/ziaulhasanhamim) | Frontend |
## Requirements

- A C++17 compiler
- CMake 3.16 or newer
- [Crow](https://crowcpp.org/)

The build creates `codeforge_backend`.

## macOS

Install Xcode Command Line Tools and [Homebrew](https://brew.sh/) if they are not already installed. From Terminal:

```sh
xcode-select --install
brew install cmake crow
```

If `xcode-select --install` reports that the tools are already installed, continue. Extract the project, open Terminal in the `backend` folder containing `CMakeLists.txt`, then run:

```sh
cd backend
cmake -S . -B build
cmake --build build --parallel
./build/codeforge_backend
```

The server listens on port **18080**. Leave that Terminal window running while sending requests. Stop it with `Ctrl+C`.

## Windows (PowerShell, Visual Studio compiler)

Install [Git](https://git-scm.com/download/win), [CMake](https://cmake.org/download/), and Visual Studio or Build Tools with the **Desktop development with C++** workload. In PowerShell, install [vcpkg](https://learn.microsoft.com/vcpkg/get_started/get-started) and Crow:

```powershell
git clone https://github.com/microsoft/vcpkg.git "$env:USERPROFILE\vcpkg"
& "$env:USERPROFILE\vcpkg\bootstrap-vcpkg.bat"
& "$env:USERPROFILE\vcpkg\vcpkg.exe" install crow:x64-windows
```

Extract the project and open PowerShell in its `backend` folder. Configure and build with the vcpkg toolchain:
Run this in Developer PowerShell
```powershell
cmake -S . -B build -DVCPKG_TARGET_TRIPLET=x64-windows "-DCMAKE_TOOLCHAIN_FILE=$env:USERPROFILE/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
.\build\Release\codeforge_backend.exe
```

The `-A x64` architecture must match `crow:x64-windows`. If CMake cannot find Crow, check the vcpkg path and rerun configuration in a fresh build directory. Stop the server with `Ctrl+C`.

## Send an analysis request

With the server running, save this as `request.json` in another terminal's current directory:

```json
{
  "source_code": "int algorithm(int n) { for (int i = 0; i < n; ++i) {} return 0; }"
}
```

On macOS:

```sh
curl -X POST http://localhost:18080/analysis-complexity \
  -H 'Content-Type: application/json' \
  --data-binary @request.json
```

On Windows PowerShell:

```powershell
curl.exe -X POST http://localhost:18080/analysis-complexity -H "Content-Type: application/json" --data-binary "@request.json"
```

A response for this example has the following shape (the exact estimates depend on the submitted code):

```json
{
  "timeComplexity": "O(n)",
  "spaceComplexity": "O(1)",
  "detectedStructures": []
}
```

The request must contain a JSON string field named `source_code`. Name the entry function to analyze `algorithm`; other functions may be helpers. An invalid JSON body receives HTTP 400. You can also use the supplied `backend.http` file with an HTTP client such as the VS Code REST Client extension.

## Source layout

| Path | Purpose |
| --- | --- |
| `src/main.cpp` | Crow route and JSON response |
| `src/codeforge/` | Tokenizer, analyzer, call graph, and complexity logic |
| `include/codeforge/` | Public engine headers |
| `include/codeforge/dsa/` | Custom stack, queue, linked list, binary search tree set, and merge sort |
| `backend.http` | Sample HTTP request |

The analyzer scans code as text; it does not compile or execute the submitted program. `O(?)` means the current heuristics could not produce a supported estimate.
