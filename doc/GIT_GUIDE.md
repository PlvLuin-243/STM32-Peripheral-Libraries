# Git Guide - Hướng dẫn sử dụng Git cho dự án STM32

## 📋 Mục lục
- [Clone Repository](#clone-repository)
- [Pull - Cập nhật code](#pull---cập-nhật-code)
- [Push - Đẩy code lên remote](#push---đẩy-code-lên-remote)
- [Skip Worktree - Bỏ qua theo dõi file](#skip-worktree---bỏ-qua-theo-dõi-file)
- [Commit - Lưu thay đổi](#commit---lưu-thay-đổi)
- [Workflow cơ bản](#workflow-cơ-bản)

## 🔄 Clone Repository

### Clone dự án về máy local
```bash
# Clone repository từ GitHub
git clone https://github.com/username/repository-name.git

# Di chuyển vào thư mục dự án
cd repository-name
```

### Clone với branch cụ thể
```bash
git clone -b branch-name https://github.com/username/repository-name.git
```

## ⬇️ Pull - Cập nhật code

### Pull từ remote repository
```bash
# Cập nhật code từ branch hiện tại
git pull

# Pull từ branch cụ thể
git pull origin main
git pull origin develop
```

### Pull với rebase (khuyến nghị)
```bash
# Pull và rebase thay vì merge
git pull --rebase origin main
```

## ⬆️ Push - Đẩy code lên remote

### Push lần đầu tiên (set upstream)
```bash
# Push và set upstream branch
git push -u origin main
git push -u origin develop

# Hoặc push branch mới
git push -u origin feature/new-feature
```

### Push thông thường
```bash
# Push sau khi đã set upstream
git push

# Push force (cẩn thận khi dùng)
git push --force-with-lease
```

### Tạo và push branch mới
```bash
# Tạo branch mới
git checkout -b feature/can-communication

# Push branch mới lên remote
git push -u origin feature/can-communication
```

## 🚫 Skip Worktree - Bỏ qua theo dõi file

### Bỏ qua theo dõi file (nhưng giữ trên remote)
```bash
# Bỏ qua theo dõi file cụ thể
git update-index --skip-worktree CAN_MASTER/CAN_MASTER.uvguix.PC
git update-index --skip-worktree CAN_MASTER/CAN_MASTER.uvprojx

# Bỏ qua nhiều file cùng lúc
git update-index --skip-worktree file1.txt file2.txt
```

### Xem danh sách file đang skip-worktree
```bash
# Liệt kê các file đang được skip
git ls-files -v | grep ^S
```

### Bật lại theo dõi file
```bash
# Bật lại theo dõi file
git update-index --no-skip-worktree CAN_MASTER/CAN_MASTER.uvguix.PC
git update-index --no-skip-worktree CAN_MASTER/CAN_MASTER.uvprojx
```

### Use case phổ biến cho STM32/Keil
```bash
# Bỏ qua các file IDE settings của Keil
git update-index --skip-worktree **/*.uvguix.*
git update-index --skip-worktree **/*.uvopt.*
```

## 💾 Commit - Lưu thay đổi

### Commit cơ bản
```bash
# Add file vào staging area
git add .
git add specific-file.c

# Commit với message
git commit -m "Add CAN communication module"
```

### Commit best practices
```bash
# Commit với message chi tiết
git commit -m "feat: implement CAN bus communication

- Add CAN initialization function
- Implement message transmission
- Add error handling for CAN errors
- Update project documentation"

# Commit với co-author
git commit -m "fix: resolve CAN timeout issue

Co-authored-by: John Doe <john@example.com>"
```

### Commit message conventions
```bash
# Feature mới
git commit -m "feat: add new feature"

# Bug fix
git commit -m "fix: resolve bug description"

# Documentation
git commit -m "docs: update README with setup instructions"

# Refactor
git commit -m "refactor: improve code structure"

# Test
git commit -m "test: add unit tests for CAN module"

# Build/CI
git commit -m "build: update Keil project settings"
```

### Amend commit (sửa commit cuối)
```bash
# Sửa message commit cuối
git commit --amend -m "New commit message"

# Thêm file vào commit cuối
git add forgotten-file.c
git commit --amend --no-edit

# Force push với safety check
git push --force-with-lease origin main
```

## 🔄 Workflow cơ bản

### Workflow hàng ngày
```bash
# 1. Cập nhật code mới nhất
git pull --rebase origin main

# 2. Tạo branch cho feature mới
git checkout -b feature/uart-driver

# 3. Làm việc và commit
git add .
git commit -m "feat: implement UART driver"

# 4. Push branch lên remote
git push -u origin feature/uart-driver

# 5. Tạo Pull Request trên GitHub

# 6. Sau khi merge, quay về main và cập nhật
git checkout main
git pull origin main

# 7. Xóa branch local (optional)
git branch -d feature/uart-driver
```

### Workflow khi có conflict
```bash
# 1. Pull và gặp conflict
git pull origin main

# 2. Giải quyết conflict trong editor

# 3. Add các file đã resolve
git add .

# 4. Commit merge
git commit -m "resolve: merge conflicts in main.c"

# 5. Push
git push
```

## 📁 File cần ignore cho STM32

```gitignore
# Keil uVision generated files
*.uvguix
*.uvguix.*
*.uvopt
*.uvopt.*
*.uvoptx
*.uvoptx.*

# Build files
Objects/
Listings/
DebugConfig/
*.o
*.d
*.axf
*.hex
*.bin
*.map

# IDE files
.vscode/
*.bak
```

## 🚨 Lưu ý quan trọng

### ⚠️ Cẩn thận khi dùng
- `git push --force` - Có thể mất dữ liệu
- `git reset --hard` - Xóa tất cả thay đổi chưa commit
- `git clean -fd` - Xóa file untracked

### ✅ Best practices
- Luôn commit thường xuyên với message rõ ràng
- Pull trước khi push
- Sử dụng branch cho feature mới
- Review code trước khi merge
- Backup trước khi dùng lệnh destructive

## 🆘 Lệnh hữu ích khác

```bash
# Xem status
git status

# Xem history
git log --oneline

# Xem diff
git diff

# Stash thay đổi
git stash
git stash pop

# Reset về commit trước
git reset --soft HEAD~1

# Xem remote
git remote -v
```