//
// Created by ivan on 18.04.2026.
//

#ifndef MORZE_UUIDCONVERTER_H
#define MORZE_UUIDCONVERTER_H
#include <vector>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>


class UUIDConverter {
public:
    // static boost::uuids::uuid generate() {
    //     static boost::uuids::random_generator gen;
    //     return gen();
    // }

    static std::vector<char> toBlob(const boost::uuids::uuid& id) {
        return {id.begin(), id.end()};
    }

    static boost::uuids::uuid fromBlob(const std::vector<char>& blob) {
        if (blob.size() != 16) {
            throw std::invalid_argument("Blob size must be 16 bytes for UUID");
        }
        boost::uuids::uuid id;
        std::copy(blob.begin(), blob.end(), id.begin());
        return id;
    }

    static std::string toString(const boost::uuids::uuid& id) {
        return boost::uuids::to_string(id);
    }

    static boost::uuids::uuid fromString(const std::string& str) {
        boost::uuids::string_generator gen;
        try {
            return gen(str);
        } catch (const std::exception& e) {
            throw std::invalid_argument("Invalid UUID string: " + str);
        }
    }
};


#endif //MORZE_UUIDCONVERTER_H