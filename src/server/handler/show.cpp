#include "../registry.hpp"

class ShowTableHandler : public ActionHandler {
public:
    void handle(const json& task, Database::ptr db , json& response) override {
        std::string name = task["name"];
        auto containers = db->listContainers();
		for ( auto container: containers) {
			if (name == "*" || name == container->getName()) {
				response[container->getName()] = container->toJson();
			}
		}
    }
};

REGISTER_ACTION("show", ShowTableHandler)