# Hexagon - AI Assistant for Your System Tray

<p align="center">
  <a href="https://opensource.org/licenses/MIT"><img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License"></a>
  <a href="https://github.com/HarrisSagiris/Hexagon"><img src="https://img.shields.io/badge/version-1.0.0-blue.svg" alt="Version"></a>
</p>

Hexagon is a Windows system tray application that provides easy access to AI capabilities powered by Hugging Face models. With a simple right-click menu, you can interact with AI models for text generation, chat, and image tasks from anywhere on your system.

## Features

- **System Tray Integration**: Access Hexagon conveniently from the Windows system tray
- **Multiple AI Windows**: Dedicated windows for chat, text generation, and image tasks
- **Model Selection**: Choose from various Hugging Face models through the context menu
- **Dark Mode Support**: Toggle between light and dark themes
- **Asynchronous Processing**: Non-blocking UI with background message processing

## Tech Stack

- Windows API (Win32)
- C++ Standard Library
- GDI+ for graphics
- WinHTTP for API calls
- Hugging Face Inference API

## Installation

1. Download the latest release from the Releases page
2. Run the executable
3. Configure your Hugging Face API token (default: "hf_your_token_here")

## Usage

1. Right-click the hexagon icon in your system tray
2. Select your desired interaction mode:
   - Chat Window
   - Image Window  
   - Text Window
3. Choose a model from the context menu
4. Enter your prompt and get AI-generated responses

## Building from Source

### Prerequisites

- Visual Studio with C++ support
- Windows SDK
- CMake (optional)

### Build Steps

1. Clone the repository
2. Open in Visual Studio 
3. Build the solution

## Configuration

Edit the config file to customize:
- API token
- Selected model
- Dark/light mode preference

## Contributing

1. Fork the repository
2. Create a feature branch
3. Submit a pull request

## License

MIT License - see LICENSE file for details

## Contact

Create an issue on GitHub for bug reports or feature requests.
