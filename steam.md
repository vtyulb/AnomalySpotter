[h1]AnomalySpotter — practice tool for "Observation Duty"-style games[/h1]

If you play camera-monitoring anomaly games and want to get faster at spotting
"what changed in this room," I made a small external tool that helps you train
exactly that skill.

I built it first for [b]Dead Signal[/b], but it does NOT read the game's memory or
files — it just compares what's on your screen. That means it works in pretty much
any [b]Observation Duty[/b]-style game (I'm on Observation Duty, Dead Signal, and
similar camera/anomaly titles).

[ ============== INSERT SCREENSHOT 1: main window ============== ]

[h1]How it works[/h1]
[list]
[*][b]F5[/b] — save the current screen as a "clean" reference of a room/camera.
[*]It then compares the live screen to every saved reference [b]5 times a second[/b]
(every 200 ms) and shows the percentage difference, so you instantly see when
something is off.
[*][b]On-screen HUD[/b] — a small number in the corner of the game shows the
difference vs the closest reference. It turns red when something starts to differ,
and blinks once the change is clearly significant. If it goes gray, no saved
reference matches what you're looking at (you probably need a fresh snapshot here).
[*][b]F6[/b] — highlights exactly what changed compared to the most similar
reference, so you can confirm the anomaly. Press again to hide. Prefer it
old-school? There's also a hold-to-show mode, and a blink mode that flashes
the highlight over the live picture so it's impossible to miss.
[/list]

Hotkeys are rebindable if F5/F6 clash with your game.

[ ============== INSERT SCREENSHOT 2: HUD over the game ============== ]

[ ============== INSERT SCREENSHOT 3: F6 difference highlight ============== ]

It's meant as a [b]training aid[/b] — use it to build the habit of catching subtle
changes, then wean yourself off it. Multi-monitor is supported (pick the screen the
game runs on), and there's a sensitivity slider to ignore noise from the cursor,
compression and small animations.

[h1]Platforms[/h1]
Tested on [b]Windows[/b] and [b]Arch Linux / Wayland[/b]. (Yes, I use Arch btw.)
On Linux the system asks once to allow the global hotkeys — just confirm.

[h1]Download[/h1]
Free and open source (MIT). Windows ZIP and source code:
[ ============== INSERT GITHUB LINK ============== ]
No installation needed on Windows — unzip and run.

[h1]Recommended setup[/h1]
Run the game in [b]borderless windowed[/b] mode. The overlay/highlight is drawn on
top of the game, and that won't show over an exclusive-fullscreen window — borderless
fixes it and costs you nothing in these games.

Have fun, and let me know if it helps your scores!
