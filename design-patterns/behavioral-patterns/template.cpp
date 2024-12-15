#include <iostream>

class DataMiner {
    public:
        virtual ~DataMiner() = default;
        virtual void extractFile() = 0;

        void  parseData()  { std::cout <<         "Parsing extracted data..\n"; }
        void analyseData() { std::cout << "Analysing parsed data contents..\n"; }
        void  sendReport() { std::cout <<   "Sending analysed data report..\n"; }

        void mine() {
            extractFile();    
            parseData();
            analyseData();
            sendReport();
        }
};

class PDFDataMiner: public DataMiner {
    void extractFile() override { std::cout << "Extracting PDF data..\n"; }
};

class CSVDataMiner: public DataMiner {
    void extractFile() override { std::cout << "Extracting CSV data..\n"; }
};

class DOCDataMiner: public DataMiner {
    void extractFile() override { std::cout << "Extracting DOC data..\n"; }
};

int main() {
    DataMiner *pdf {new PDFDataMiner()};
    pdf->mine();
    std::cout << "\n";
    delete pdf;

    DataMiner *csv {new CSVDataMiner()};
    csv->mine();
    std::cout << "\n";
    delete csv;

    DataMiner *doc {new DOCDataMiner()};
    doc->mine();
    std::cout << "\n";
    delete doc;
}
