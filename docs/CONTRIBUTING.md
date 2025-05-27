# Contributing to ThermortoZigBee

First off, thank you for considering contributing to ThermortoZigBee! It's people like you that make this project better for everyone.

## 🤝 Code of Conduct

This project and everyone participating in it is governed by our Code of Conduct. By participating, you are expected to uphold this code. Please be respectful and professional in all interactions.

## 🔒 Security First

Given the nature of this project (230V AC modifications), **safety is our top priority**. Any contribution that could compromise safety will be rejected.

### Safety Guidelines for Contributors

1. **Never** suggest modifications that reduce isolation or safety margins
2. **Always** include appropriate warnings in documentation
3. **Test** thoroughly before submitting electrical changes
4. **Document** safety considerations for any hardware changes

## 🚀 How Can I Contribute?

### Reporting Bugs

Before creating bug reports, please check existing issues to avoid duplicates.

**When reporting bugs, include:**
- Detailed description of the issue
- Steps to reproduce
- Expected behavior
- Actual behavior
- Hardware version (ESP32-C6 module type)
- Firmware version
- Logs (with sensitive data removed)
- Photos of your setup (if relevant)

### Suggesting Enhancements

Enhancement suggestions are welcome! Please provide:
- Clear use case
- Why this enhancement would be useful
- Possible implementation approach
- Any safety considerations

### Code Contributions

1. **Fork** the repository
2. **Create** a feature branch (`git checkout -b feature/AmazingFeature`)
3. **Commit** your changes (`git commit -m 'Add some AmazingFeature'`)
4. **Push** to the branch (`git push origin feature/AmazingFeature`)
5. **Open** a Pull Request

## 📝 Development Setup

### Prerequisites

```bash
# ESP-IDF v5.2+
git clone -b v5.2 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32c6
. ./export.sh

# KiCad 7.0+ for PCB modifications
# Install from: https://www.kicad.org/
```

### Building the Firmware

```bash
cd firmware
idf.py set-target esp32c6
idf.py build
```

### Running Tests

```bash
# Unit tests
cd firmware/test
idf.py build
idf.py flash monitor

# Integration tests require actual hardware
```

## 🎨 Coding Standards

### C Code (Firmware)

- Follow ESP-IDF coding style
- Use meaningful variable names
- Comment complex logic
- Keep functions small and focused
- Always check return values

```c
// Good
esp_err_t thermor_set_temperature(float temp_celsius)
{
    if (temp_celsius < TEMP_MIN || temp_celsius > TEMP_MAX) {
        ESP_LOGE(TAG, "Temperature %.1f out of range", temp_celsius);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Update internal state with mutex protection
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    g_state.target_temp = temp_celsius;
    xSemaphoreGive(g_state_mutex);
    
    return ESP_OK;
}
```

### Documentation

- Use Markdown for all docs
- Include code examples
- Add diagrams where helpful
- Keep language clear and concise
- Always include safety warnings where relevant

### Hardware Design

- Follow IPC standards for PCB design
- Maintain proper isolation distances
- Use standard footprints where possible
- Document all custom footprints
- Include 3D models when available

## 🔄 Pull Request Process

1. **Update** documentation with details of changes
2. **Update** the README.md with new features/changes
3. **Increase** version numbers following [SemVer](http://semver.org/)
4. **Ensure** all tests pass
5. **Request** review from maintainers

### PR Checklist

- [ ] Code compiles without warnings
- [ ] Documentation is updated
- [ ] Tests are passing
- [ ] Safety implications considered
- [ ] Backwards compatibility maintained
- [ ] Commit messages are clear

## 🏷️ Versioning

We use [SemVer](http://semver.org/) for versioning:
- MAJOR: Incompatible API changes
- MINOR: New functionality (backwards compatible)
- PATCH: Bug fixes (backwards compatible)

## 📋 Issue Labels

- `bug`: Something isn't working
- `enhancement`: New feature request
- `safety`: Safety-related issue (PRIORITY)
- `documentation`: Documentation improvements
- `hardware`: PCB or component related
- `firmware`: ESP32 code related
- `good first issue`: Good for newcomers
- `help wanted`: Extra attention needed

## 🌍 Translations

Help us translate the project:
1. Copy `docs/en/` to `docs/YOUR_LANGUAGE/`
2. Translate all `.md` files
3. Update links to point to translated versions
4. Submit PR with title: `Add YOUR_LANGUAGE translation`

## 💡 Feature Ideas

Current wishlist:
- [ ] OTA updates
- [ ] Energy monitoring dashboard
- [ ] Machine learning for optimal heating
- [ ] Voice control integration
- [ ] Mobile app
- [ ] Multi-zone support

## 📞 Getting Help

- GitHub Issues for bugs/features
- Discussions for questions
- Discord for real-time chat
- Email for security issues: security@[project-domain]

## 🙏 Recognition

Contributors will be recognized in:
- README.md contributors section
- Release notes
- Project website (when available)

## 📄 License

By contributing, you agree that your contributions will be licensed under the MIT License.

---

Thank you for helping make home heating smarter and more efficient! 🔥🏠