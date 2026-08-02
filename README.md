https://github.com/user-attachments/assets/9eaf7c3a-a5db-41a3-a7dd-e00eba75456d

A Hyprland plugin that automatically resizes, centers, and cascades floating windows to keep your workspace organized.

## Installation

### hyprpm
```bash
hyprpm add https://github.com/syndicateF/smartfloat.git
hyprpm enable smartfloat
```

## Configuration
Add this to your `hyprland.lua`:
```lua
hl.config({
    plugin = {
        smartfloat = {
            target_width = 1000,
            target_height = 600,
            cascade_step = 25,
        },
    },
})

hl.bind("SUPER + SPACE", function()
    hl.plugin.smartfloat.toggle()
end)
```
