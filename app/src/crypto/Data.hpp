#include <stdio.h>
#include <string>
#pragma once

class Data {
public:
    Data(): Data(0) {};
    Data(unsigned char* bytes, size_t size);
    Data(char* bytes, size_t size): Data((unsigned char *)bytes, size) {};
    Data(size_t capacity);
    
    ~Data();
    
    unsigned char* bytes() const {
        return m_bytes;
    }
    
    size_t size() const {
        return m_size;
    }
    
    Data subdata(size_t start, size_t size);
    Data append(Data other);
    
    Data(const Data& that);
    Data& operator=(const Data& that);
    
    static Data random_bytes(size_t size);
    static Data read_from_file(std::string path);
    void write_to_file(std::string path);
    
    Data hex_to_bytes() const;
    Data hex() const;
    
    bool is_empty() const {
        return m_size == 0;
    }
    
private:
    unsigned char* m_bytes = nullptr;
    size_t m_size = 0;
};
