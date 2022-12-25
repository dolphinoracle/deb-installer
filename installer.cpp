/**********************************************************************
 *  installer.cpp
 **********************************************************************
 * Copyright (C) 2022 MX Authors
 *
 * Authors: Adrian
 *          MX Linux <http://mxlinux.org>
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package. If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/
#include "installer.h"

#include <QApplication>
#include <QDebug>
#include <QFileDialog>
#include <QGridLayout>
#include <QMessageBox>

#include "cmd.h"

Installer::Installer(const QCommandLineParser &arg_parser)
{
    QStringList file_names = canonicolize(arg_parser.positionalArguments());
    if (confirmAction(file_names))
        install(file_names);
}

QStringList Installer::canonicolize(const QStringList &file_names)
{
    QStringList new_list;
    new_list.reserve(file_names.size());
    for (auto const &name : file_names)
        new_list << "\"" + QFileInfo(name).canonicalFilePath() + "\"";
    return new_list;
}

Installer::~Installer() = default;

bool Installer::confirmAction(const QStringList &names)
{
    QString detailed_names;
    QString detailed_removed_names;
    QString detailed_to_install;
    QString msg;
    QString names_str = names.join(" ");
    QStringList detailed_installed_names;

    const QString frontend = QStringLiteral("DEBIAN_FRONTEND=$(dpkg -l debconf-kde-helper 2>/dev/null | grep -sq ^i "
                                            "&& echo kde || echo gnome) LANG=C ");
    const QString aptget = QStringLiteral("apt-get -s -V -o=Dpkg::Use-Pty=0 ");

    detailed_names = cmd.getCmdOut(
        frontend + aptget + "install " + names_str
        + R"lit(|grep 'Inst\|Remv'| awk '{V=""; P="";}; $3 ~ /^\[/ { V=$3 }; $3 ~ /^\(/ { P=$3 ")"}; $4 ~ /^\(/ {P=" => " $4 ")"}; {print $2 ";" V  P ";" $1}')lit");
    if (!detailed_names.isEmpty())
        detailed_installed_names = detailed_names.split(QStringLiteral("\n"));
    detailed_installed_names.sort();
    QStringListIterator iterator(detailed_installed_names);
    QString value;
    while (iterator.hasNext()) {
        value = iterator.next();
        if (value.contains(QLatin1String("Remv"))) {
            value = value.section(QStringLiteral(";"), 0, 0) + " " + value.section(QStringLiteral(";"), 1, 1);
            detailed_removed_names += value + "\n";
        }
        if (value.contains(QLatin1String("Inst"))) {
            value = value.section(QStringLiteral(";"), 0, 0) + " " + value.section(QStringLiteral(";"), 1, 1);
            detailed_to_install += value + "\n";
        }
    }
    if (!detailed_removed_names.isEmpty())
        detailed_removed_names.prepend(tr("Remove") + "\n");
    if (!detailed_to_install.isEmpty())
        detailed_to_install.prepend(tr("Install") + "\n");

    msg = "<b>"
          + tr("The following packages will be installed. Click 'Show Details...' for information about the packages.")
          + "</b>";

    QMessageBox msgBox;
    msgBox.setText(msg);

    QString detailed_text;
    for (const auto &file_name : names) {
        detailed_text += tr("File: %1").arg(file_name) + "\n\n";
        detailed_text += cmd.getCmdOut("dpkg -I " + file_name + "| sed -n '/Package:/,$p'");
        detailed_text += "\n\n";
    }
    msgBox.setDetailedText(detailed_text);

    if (!detailed_installed_names.isEmpty() || !detailed_removed_names.isEmpty())
        msgBox.setInformativeText(detailed_to_install + "\n" + detailed_removed_names);
    else
        msgBox.setInformativeText(tr("Will install the following:") + "\n" + names.join("\n"));

    msgBox.addButton(tr("Install"), QMessageBox::AcceptRole);
    msgBox.addButton(QMessageBox::Cancel);

    auto *horizontalSpacer = new QSpacerItem(600, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    auto *layout = qobject_cast<QGridLayout *>(msgBox.layout());
    layout->addItem(horizontalSpacer, 0, 1);
    return msgBox.exec() == QMessageBox::AcceptRole;
}

void Installer::install(const QStringList &file_names)
{
    cmd.run("x-terminal-emulator -e bash -c 'echo Installing selected package please authenticate; echo; pkexec apt "
            "reinstall "
            + file_names.join(" ") + "; echo; read -n1 -srp \"" + tr("Press any key to close") + "\"'");
}
