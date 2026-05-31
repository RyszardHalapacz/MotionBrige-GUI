#include "pipeline_options.h"

#include <QFile>
#include <QTextStream>

/* --------------------------------------------------------------------------
 *  Parser wcięć YAML:
 *
 *  frontends:           indent=0
 *    - id: ...          indent=2
 *      label: ...       indent=4
 *
 *  backends:            indent=0
 *    - id: ...          indent=2
 *      label: ...       indent=4
 *      robots:          indent=4
 *        - id: ...      indent=6
 *          label: ...   indent=8
 * --------------------------------------------------------------------------*/

static int countIndent(const QString& line)
{
    int n = 0;
    for (QChar c : line) { if (c == ' ') n++; else break; }
    return n;
}

std::optional<PipelineOptions> PipelineOptionsLoader::load(const QString& resourcePath)
{
    QFile f(resourcePath);
    if (!f.open(QFile::ReadOnly | QFile::Text))
        return std::nullopt;

    PipelineOptions opts;

    enum class Top { None, Frontends, Backends };
    Top top       = Top::None;
    bool inRobots = false;

    PipelineOption pendingFrontend;
    BackendOption  pendingBackend;
    PipelineOption pendingRobot;
    bool hasFrontend = false;
    bool hasBackend  = false;
    bool hasRobot    = false;

    auto commitRobot = [&]() {
        if (hasRobot && !pendingRobot.id.isEmpty() && !pendingRobot.label.isEmpty())
            pendingBackend.robots.append(pendingRobot);
        pendingRobot = {};
        hasRobot = false;
    };

    auto commitBackend = [&]() {
        commitRobot();
        if (hasBackend && !pendingBackend.id.isEmpty() && !pendingBackend.label.isEmpty())
            opts.backends.append(pendingBackend);
        pendingBackend = {};
        hasBackend = false;
        inRobots   = false;
    };

    auto commitFrontend = [&]() {
        if (hasFrontend && !pendingFrontend.id.isEmpty() && !pendingFrontend.label.isEmpty())
            opts.frontends.append(pendingFrontend);
        pendingFrontend = {};
        hasFrontend = false;
    };

    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line    = in.readLine();
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#')) continue;

        const int indent = countIndent(line);

        /* Nagłówek sekcji top-level */
        if (indent == 0 && trimmed.endsWith(':')) {
            commitFrontend();
            commitBackend();
            const QString name = trimmed.chopped(1);
            if      (name == "frontends") top = Top::Frontends;
            else if (name == "backends")  top = Top::Backends;
            else                          top = Top::None;
            continue;
        }

        if (top == Top::None) continue;

        /* "    robots:" wewnątrz backendu */
        if (top == Top::Backends && indent == 4 && trimmed == "robots:") {
            inRobots = true;
            continue;
        }

        /* Nowy element listy indent=2 → frontend lub backend */
        if (indent == 2 && trimmed.startsWith("- id:")) {
            if (top == Top::Frontends) {
                commitFrontend();
                pendingFrontend.id = trimmed.mid(5).trimmed();
                hasFrontend = true;
            } else {
                commitBackend();
                pendingBackend.id = trimmed.mid(5).trimmed();
                hasBackend  = true;
                inRobots    = false;
            }
            continue;
        }

        /* Nowy robot indent=6 */
        if (top == Top::Backends && inRobots && indent == 6 && trimmed.startsWith("- id:")) {
            commitRobot();
            pendingRobot.id = trimmed.mid(5).trimmed();
            hasRobot = true;
            continue;
        }

        /* label: */
        if (trimmed.startsWith("label:")) {
            const QString val = trimmed.mid(6).trimmed();
            if (top == Top::Frontends && hasFrontend && indent == 4)
                pendingFrontend.label = val;
            else if (top == Top::Backends && !inRobots && hasBackend && indent == 4)
                pendingBackend.label = val;
            else if (top == Top::Backends && inRobots && hasRobot && indent == 8)
                pendingRobot.label = val;
        }
    }

    commitFrontend();
    commitBackend();

    if (opts.frontends.isEmpty() || opts.backends.isEmpty())
        return std::nullopt;

    return opts;
}

PipelineOptions PipelineOptionsLoader::fallback()
{
    PipelineOptions opts;

    opts.frontends = {
        {"gcode_printer3d", "GCode 3D Printer"},
        {"gcode_cnc",       "GCode CNC"},
    };

    BackendOption kuka;
    kuka.id     = "kuka_krl";
    kuka.label  = "KUKA KRL";
    kuka.robots = {
        {"kuka_krl_kr640",   "KR 640 R2800-2"},
        {"kuka_krl_kr4r600", "KR 4 R600"},
    };

    BackendOption ur;
    ur.id    = "urscript";
    ur.label = "URScript";
    // robots: puste — silnik nie rozróżnia modeli UR (jeden backend URScript)

    BackendOption pseudo;
    pseudo.id    = "pseudo_robot_3d";
    pseudo.label = "Pseudo Robot 3D";

    BackendOption dbg;
    dbg.id    = "debug";
    dbg.label = "Debug";

    opts.backends = {kuka, ur, pseudo, dbg};
    return opts;
}
