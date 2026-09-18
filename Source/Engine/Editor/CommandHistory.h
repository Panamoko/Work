#pragma once
#include <vector>
#include <memory>

namespace uel {

    class Command {
    public:
        virtual ~Command() = default;
        virtual void Undo() = 0;
        virtual void Redo() = 0;
    };

    class CommandHistory {
    public:
        void Push(std::unique_ptr<Command> cmd) {   // Šù‚É“K—pÏ‚Ý‚Ì•ÏX‚ðÏ‚Þ
            undoStack.push_back(std::move(cmd));
            redoStack.clear();
        }
        void Undo() {
            if (undoStack.empty()) return;
            auto cmd = std::move(undoStack.back()); undoStack.pop_back();
            cmd->Undo();
            redoStack.push_back(std::move(cmd));
        }
        void Redo() {
            if (redoStack.empty()) return;
            auto cmd = std::move(redoStack.back()); redoStack.pop_back();
            cmd->Redo();
            undoStack.push_back(std::move(cmd));
        }
        bool CanUndo() const { return !undoStack.empty(); }
        bool CanRedo() const { return !redoStack.empty(); }
        void Clear() { undoStack.clear(); redoStack.clear(); }


    private:
        std::vector<std::unique_ptr<Command>> undoStack;
        std::vector<std::unique_ptr<Command>> redoStack;
    };

} // namespace uel