#ifndef IOBJECT_HPP
#define IOBJECT_HPP

class IObject {
public:
    using ptr = std::shared_ptr<IObject>;

    // Delete copy constructor and copy assignment operator
    IObject(const IObject&) = delete;
    IObject& operator=(const IObject&) = delete;

    // Default constructor (move semantics can be allowed)
    IObject() = default;

    // Move constructor and move assignment operator (optional, if needed)
    IObject(IObject&&) = default;
    IObject& operator=(IObject&&) = default;

};

#endif 
