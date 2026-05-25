#pragma once

#include "commands/Command.h"

#include <QObject>

#include <memory>
#include <vector>

class CommandManager final : public QObject {
    Q_OBJECT

public:
    explicit CommandManager(QObject *parent = nullptr);

    void execute(std::unique_ptr<Command> command);
    void undo();
    void redo();
    void clear();

    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    [[nodiscard]] QStringList undoHistory() const;

Q_SIGNALS:
    void stackChanged();
    void commandApplied(const QString &name);

private:
    std::vector<std::unique_ptr<Command>> undoStack_;
    std::vector<std::unique_ptr<Command>> redoStack_;
};
