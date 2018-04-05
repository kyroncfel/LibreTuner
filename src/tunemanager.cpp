#include "tunemanager.h"
#include "libretuner.h"
#include "tune.h"

#include <cassert>

TuneManager * TuneManager::get()
{
    static TuneManager manager;
    return &manager;
}



TuneManager::TuneManager()
{
}



TunePtr TuneManager::createTune(RomPtr base, const std::string& name)
{
    assert(base);
    
    LibreTuner::get()->checkHome();
    
    QString tuneRoot = LibreTuner::get()->home() + "/tunes/";
    QString path = QString::fromStdString(name);
    if (QFile::exists(path))
    {
        int count = 0;
        do
        {
            path = QString::fromStdString(name) + QString::number(++count);
        } while (QFile::exists(tuneRoot + path));
    }
    
    QFile file(tuneRoot + path);
    if (!file.open(QFile::WriteOnly))
    {
        lastError_ = file.errorString();
        return nullptr;
    }
    
    QXmlStreamWriter xml(&file);
    xml.writeStartDocument();
    xml.writeDTD("<!DOCTYPE tune>");
    xml.writeEndDocument();
    file.close();
    
    TunePtr tune = std::make_shared<Tune>();
    tune->setName(name);
    tune->setPath(path.toStdString());
    tune->setBase(base);
    
    tunes_.push_back(tune);
    emit updateTunes();
    
    if (!save())
    {
        return nullptr;
    }
    
    return tune;
}



void TuneManager::readTunes(QXmlStreamReader& xml)
{
    assert(xml.isStartElement() && xml.name() == "tunes");
    tunes_.clear();
    
    while (xml.readNextStartElement())
    {
        if (xml.name() != "tune")
        {
            xml.raiseError("Unexpected element in tunes");
            return;
        }
        
        
        TunePtr tune = std::make_shared<Tune>();
        
        // Read ROM data
        while (xml.readNextStartElement())
        {
            if (xml.name() == "name")
            {
                tune->setName(xml.readElementText().toStdString());
            }
            if (xml.name() == "path")
            {
                tune->setPath(xml.readElementText().toStdString());
            }
            if (xml.name() == "base")
            {
                // Locate the base rom from the ID
                bool ok;
                int id = xml.readElementText().toInt(&ok);
                if (!ok)
                {
                    xml.raiseError("Base is not a valid decimal number");
                    return;
                }
                
                tune->setBase(RomManager::get()->fromId(id));
            }
        }
        
        tunes_.push_back(tune);
    }
}



bool TuneManager::save()
{
    LibreTuner::get()->checkHome();
    
    QString listPath = LibreTuner::get()->home() + "/" + "tunes.xml";
    
    QFile listFile(listPath);
    if (!listFile.open(QFile::WriteOnly))
    {
        lastError_ = listFile.errorString();
        return false;
    }
    
    QXmlStreamWriter xml(&listFile);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(-1); // tabs > spaces
    
    xml.writeStartDocument();
    xml.writeDTD("<!DOCTYPE tunes>");
    xml.writeStartElement("tunes");
    for (const TunePtr tune : tunes_)
    {
        xml.writeStartElement("tune");
        xml.writeTextElement("name", QString::fromStdString(tune->name()));
        xml.writeTextElement("path", QString::fromStdString(tune->path()));
        
        xml.writeEndElement();
    }
    
    xml.writeEndDocument();
    return true;
}



bool TuneManager::load()
{
    LibreTuner::get()->checkHome();
    
    QString listPath = LibreTuner::get()->home() + "/" + "tunes.xml";
    
    if (!QFile::exists(listPath))
    {
        return true;
    }
    
    QFile listFile(listPath);
    if (!listFile.open(QFile::ReadOnly))
    {
        lastError_ = listFile.errorString();
        return false;
    }
    
    QXmlStreamReader xml(&listFile);
    
    if (xml.readNextStartElement()) 
    {
        if (xml.name() == "tunes")
        {
            readTunes(xml);
        }
        else
        {
            xml.raiseError(QObject::tr("This file is not a tune list document"));
        }
    }
    
    if (xml.error())
    {
        lastError_ = QObject::tr("%1\nLine %2, column %3")
                    .arg(xml.errorString())
                    .arg(xml.lineNumber())
                    .arg(xml.columnNumber());
        return false;
    }
    
    emit updateTunes();
    
    return true;
}
