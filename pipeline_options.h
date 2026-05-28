#ifndef PIPELINE_OPTIONS_H
#define PIPELINE_OPTIONS_H

#include <QList>
#include <QString>
#include <optional>

struct PipelineOption {
    QString id;
    QString label;
};

struct BackendOption {
    QString id;
    QString label;
    QList<PipelineOption> robots;
};

struct PipelineOptions {
    QList<PipelineOption> frontends;
    QList<BackendOption>  backends;
};

class PipelineOptionsLoader {
public:
    static std::optional<PipelineOptions> load(const QString& resourcePath);
    static PipelineOptions fallback();
};

#endif // PIPELINE_OPTIONS_H
