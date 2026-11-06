# Быстрая настройка GitHub репозитория для автосборки
# Выполни в PowerShell от админа в папке проекта:

# 1. Инициализируй git (если не установлен: winget install Git.Git)
git init
git add .
git commit -m "Initial commit: FlatOut 2 Trainer"

# 2. Создай репозиторий на GitHub (нужен токен с правами repo)
# Вариант А: через gh CLI (winget install GitHub.cli)
gh auth login
gh repo create FlatOut2Trainer --public --source=. --push

# Вариант Б: вручную на github.com/new, потом:
# git remote add origin https://github.com/ТВОЙ_ЮЗЕР/FlatOut2Trainer.git
# git branch -M main
# git push -u origin main

# 3. Сборка запустится автоматически!
# Зайди в репозиторий → Actions → увидишь сборку

# 4. Артефакты скачаются после завершения (зеленая галочка):
# Actions → последний запуск → Artifacts → FlatOut2Trainer-Release x86.zip

# 5. Для релиза — просто пушь тег:
# git tag v1.0.0
# git push origin v1.0.0
# Создастся GitHub Release с .exe и .dll