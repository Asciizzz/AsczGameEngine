#include "Az3D/Billboard.hpp"
#include <algorithm>

namespace Az3D {

    size_t BillboardManager::addBillboard(const Billboard& billboard) {
        billboards.push_back(billboard);
        return billboards.size() - 1;
    }

    void BillboardManager::updateBillboard(size_t index, const Billboard& billboard) {
        if (index < billboards.size()) {
            billboards[index] = billboard;
        }
    }

    void BillboardManager::removeBillboard(size_t index) {
        if (index < billboards.size()) {
            billboards.erase(billboards.begin() + index);
        }
    }

    std::vector<size_t> BillboardManager::getBillboardsWithTexture(size_t textureIndex) const {
        std::vector<size_t> result;
        for (size_t i = 0; i < billboards.size(); ++i) {
            if (billboards[i].textureIndex == textureIndex) {
                result.push_back(i);
            }
        }
        return result;
    }

} // namespace Az3D
