
1. Инсталиране на PyInstaller

pip install pyinstaller

2. Компилиране 

# Build the standalone executable
# --onefile : Bundles everything into a single executable file
# --windowed: Hides the console window (crucial for GUI applications)
# --icon    : (Optional) Adds a custom icon to your application
# --name.     : name
/opt/homebrew/bin/python3 -m PyInstaller --noconfirm --onefile --windowed --name "STM32_Motor_Control" "modbus_gui_flash.py"

3. Къде е готовият файл?
След като процесът завърши (отнема минута-две, защото събира целия Python интерпретатор и всички зависимости в един пакет), в папката ти ще се появят няколко нови неща:

Папка build/: Тук се пазят временни файлове от компилацията. Можеш да я изтриеш след това.

Файл gui_app.spec: Това е конфигурационен файл. Ако по-късно добавяш специфични външни файлове (например конфигурационни .json файлове или картинки), ще го редактираш него.

Папка dist/ (Distribution): Тук се намира твоят готов изпълним файл!

Влизаш в папката dist/, взимаш файла gui_app.exe (или gui_app за Mac/Linux) и можеш да го пратиш на всеки – той ще работи директно, дори на компютъра да няма инсталиран Python.


4. Важна експертна бележка:
PyInstaller не е крос-компилатор. Това означава, че:

Ако пуснеш командата на Windows, ще генерира .exe.

Ако пуснеш командата на macOS, ще генерира .app / Mac изпълним файл.

Ако пуснеш командата на Linux, ще генерира Linux ELF binary.

Ако ти трябва .exe за Windows, но в момента работиш на Mac/Linux, ще трябва да пуснеш кода и компилацията в една Windows виртуална машина или на реален Windows компютър.