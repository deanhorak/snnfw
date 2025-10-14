#ifndef SNNFW_NEURAL_OBJECT_H
#define SNNFW_NEURAL_OBJECT_H

#include "snnfw/Serializable.h"
#include <cstdint>

namespace snnfw {

/**
 * @brief Base class for all neural objects in the framework
 *
 * This class provides a unique identifier for neural objects such as
 * Neurons and Clusters. The ID is a 64-bit integer that uniquely
 * identifies the object within its class.
 *
 * All NeuralObjects implement the Serializable interface for persistence.
 */
class NeuralObject : public Serializable {
public:
    /**
     * @brief Virtual destructor for proper cleanup of derived classes
     */
    virtual ~NeuralObject() = default;

    /**
     * @brief Get the unique identifier for this neural object
     * @return 64-bit unique identifier
     */
    uint64_t getId() const { return id; }

protected:
    /**
     * @brief Protected constructor - only derived classes can create instances
     * @param objectId The unique identifier for this object
     */
    explicit NeuralObject(uint64_t objectId = 0) : id(objectId) {}

    /**
     * @brief Set the unique identifier
     * @param objectId The unique identifier to set
     */
    void setId(uint64_t objectId) { id = objectId; }

private:
    uint64_t id;  ///< Unique identifier for this neural object
};

} // namespace snnfw

#endif // SNNFW_NEURAL_OBJECT_H

