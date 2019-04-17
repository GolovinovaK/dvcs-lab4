/*! \file synhra.cpp
 * Реализация функций программы
 */

#include "synhra.h"

void qError(QString error)
{
    std::wcerr << error.toStdWString() << "\n";
}

bool checkArguments(int argc, char* argv[], error &error)
{
	int a;
	int b;
	int c;
    if(argc < 3) // Нет одного и двух входных файлов
    {
        error.type = errorType::noFileIn;
        return false;
    }
    else if(argc < 4) // Нет выходного файла
    {
        error.type = errorType::noNameOut;
        return false;
    }
    return true;
}

void matchModes(const QMap<int, QString> &match, const QVector<int> &chain, QVector<QString> &mappedModes)
{
    QVector<QString> openModes; // Список открытых режимов в порядке открытия
    QString startMode = "*"; // Начальный режим, в который приходим по завершении всех режимов
    for(QVector<int>::const_iterator chainIt = chain.begin(); chainIt != chain.end(); chainIt++)
    {
        int action = *chainIt;
        QString mode = match[abs(action)]; // Название режима

        if(startMode == "") // По умолчанию первое действие становится начальным
        {
            startMode = mode;
        }

        if(action > 0) // Начало нового режима
        {
            openModes.push_back(mode);
            mappedModes.push_back(QString::number(action) + "->" + mode);
        }
        else
        {
            openModes.removeAt(openModes.lastIndexOf(mode)); // Удалим последний открытый режим такого типа
            if(!openModes.empty())
            {
                mappedModes.push_back(QString::number(action) + "->" + openModes.back()); // Перейдем в последний открытый режим
            }
            else
            {
                mappedModes.push_back(QString::number(action) + "->" + startMode); // Перейдем в стартовый режим
            }
        }
    }
}

bool readMatch(const QString &filename, QStringList &matchList, error &error)
{
    QFile matchFile(filename);
    if(!matchFile.exists())
    {
        error.type = errorType::noExistFile;
        return false;
    }

    if (!matchFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        error.type = errorType::noOpenFile;
        return false;
    }

    QTextStream in(&matchFile);
    while (!in.atEnd())
    {
        matchList.append(in.readLine());
    }
    return true;
}

bool readChain(const QString &filename, QString &stringChain, error &error)
{
    QFile chainFile(filename);
    if(!chainFile.exists())
    {
        error.type = errorType::noExistFile;
        return false;
    }

    if (!chainFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        error.type = errorType::noOpenFile;
        return false;
    }

    QTextStream in(&chainFile);
    while (!in.atEnd())
    {
        QString temp = " " + in.readLine();
        stringChain += temp;
    }
    return true;
}

bool putToMap(QStringList &matchList, QMap<int, QString> &match)
{
    if(matchList.empty())
        return false;
    bool wasError = false;
    error matchError;
    if(checkMatch(matchList,matchError))
    {
        for(QStringList::iterator it = matchList.begin(); it != matchList.end(); ++it)
        {
            QTextStream in(&(*it));
            int action;
            in >> action;
            QString mode;
            in.skipWhiteSpace();
            mode = in.readLine();
            match.insert(action, mode);
        }
    }
    else
    {
        wasError = true;
        QString errorDescription = "Файл содержит некорректные данные: в строке " + QString::number(matchError.numLine) + " ";
        if(matchError.type == errorType::noAction)
        {
            errorDescription += "отсутствует номер действия";
        }
        else if(matchError.type == errorType::noMode)
        {
            errorDescription += "отсутствует описание режима";
        }
        else if(matchError.type == errorType::duplicateAction)
        {
            errorDescription += "дублируется номер действия";
        }
        qError(errorDescription);
    }
    return !wasError;
}

bool putToVector(const QString &stringChain, QVector<int> &chain, const QMap<int, QString> &match)
{
    error chainError;
    QString copyStringChain = stringChain;
    if(!checkChain(copyStringChain,chainError))
    {
        QString errorDescription = "Файл содержит некорректные данные: в последовательность в столбце ";
        errorDescription += QString::number(chainError.numLine);
        if(chainError.type == errorType::invalidChar)
        {
            errorDescription += " входит символ, не являющийся целым числом, пробелом или минусом";
        }
		if(chainError.type == errorType::minusSplit)
        {
            errorDescription += " входит номер завершающего действия, разделенный минусом";
        }
        qError(errorDescription);
        return false;
    }

    QTextStream in(&copyStringChain);
    QVector<int> copy;
    while(!in.atEnd())
    {
        int action;
        in >> action;
        chain.push_back(action);
        copy.push_back(action);
    }

    if(chain.empty())
        return false;

    bool wasError = false;
    int deletedCount = 0;
    for(int i = 0; i < chain.length(); ++i)
    {
        int curPos = i - deletedCount;
        if(!match.contains(abs(copy[curPos])))
        {
            wasError = true;
            QString errorDescription = "Файл содержит некорректные данные: последовательность содержит номер, которому не соответствует режим под номером ";
            errorDescription += QString::number(copy[curPos]);
            qError(errorDescription);
        }
        else
        {
            if(copy[curPos] < 0)
            {
                int pos = copy.lastIndexOf(-copy[curPos],curPos); // Найдем индекс последнего соответствующего открывающего действия
                if(pos == -1)
                {
                    wasError = true;
                    QString errorDescription = "Файл содержит некорректные данные: последовательность содержит под номером ";
                    errorDescription += QString::number(i+1);
                    errorDescription += " завершающее действие ";
                    errorDescription += QString::number(-copy[curPos]);
                    errorDescription += ", не имеющее начального действия перед собой";
                    qError(errorDescription);
                }
                else
                {
                    copy.removeAt(pos); // Удалим открывающее действие
                    deletedCount++;
                    curPos--;
                }
                copy.removeAt(curPos); // Удалим закрывающее действие
                deletedCount++;
            }
        }
    }
    return !wasError;
}

bool checkMatch(QStringList & matchList, error &error)
{
    if(matchList.empty()) {
        error.type = errorType::noAction;
        error.numLine = 1;
        return false;
    }
    QMap<int,QString> mapped;

    for(QStringList::iterator it = matchList.begin(); it != matchList.end(); ++it)
    {
        QTextStream in(&(*it));
        int action;
        in >> action;
        if(in.status() != QTextStream::Ok)
        {
            error.type = errorType::noAction;
            error.numLine = (it - matchList.begin()) + 1;
            return false;
        }
        QString mode;
        mode = in.readLine();
        if(in.status() != QTextStream::Ok || mode.trimmed().length() == 0)
        {
            error.type = errorType::noMode;
            error.numLine = (it - matchList.begin()) + 1;
            return false;
        }
        if(mapped.contains(action)) {
            error.type = errorType::duplicateAction;
            error.numLine = (it - matchList.begin()) + 1;
            return false;
        }
        mapped[action] = mode;
    }
    return true;
}

bool checkChain(QString &stringChain, error &error)
{
    if(stringChain.length() == 0)
        return false;
    for(int i=0; i < stringChain.length(); ++i)
    {
        bool good_symbol = stringChain[i].isDigit() || stringChain[i] == '-' || stringChain[i].isSpace();
        bool digit_before_minus = i > 0 && stringChain[i] == '-' && stringChain[i-1].isDigit();
         if(!good_symbol)
        {
            error.type = errorType::invalidChar;
            error.numLine = i + 1;
            return false;
        }
		if(digit_before_minus)
        {
            error.type = errorType::minusSplit;
            error.numLine = i + 1;
            return false;
        }
    }
    return true;
}

bool writeData(const QVector<QString> &mappedModes, const QString &newfilename)
{
    QFile file(newfilename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    for(QVector<QString>::const_iterator it = mappedModes.begin(); it != mappedModes.end(); ++it)
    {
        out << *it << "\n";
        if(out.status() != QTextStream::Ok)
        {
            file.close();
            return false;
        }
    }

    file.close();
    return true;
}
