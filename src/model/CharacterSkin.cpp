#include "CharacterSkinNative.hpp"
#include <stdexcept>
namespace hksr::api {
struct CharacterSkin::State {
    CharacterSkinStatus status;
    std::function<std::optional<std::string>(const std::string&)> map;
    std::function<std::string(const std::string&)> sound;
    std::function<bool()> close;
    void Check(const std::string& value) const {
        if (!status.prepared || status.closed)
            throw std::logic_error("skin preparation unavailable or closed");
        if (value.find('\0') != std::string::npos)
            throw std::invalid_argument("skin resource contains NUL");
    }
};
CharacterSkin::CharacterSkin() : state_(std::make_unique<State>()) {}
CharacterSkin::~CharacterSkin() = default;
const CharacterSkinStatus& CharacterSkin::Status() const {
    return state_->status;
}
std::optional<std::string> CharacterSkin::MapResource(const std::string& value) const {
    state_->Check(value);
    return value.empty() ? std::optional<std::string>(value) : state_->map(value);
}
std::string CharacterSkin::SelectSound(const std::string& value) const {
    state_->Check(value);
    return value.empty() ? value : state_->sound(value);
}
bool CharacterSkin::Close() {
    auto& s = *state_;
    if (s.status.closed)
        return true;
    if (s.close && !s.close())
        return false;
    s.status.closed = true;
    s.status.prepared = false;
    return true;
}
std::shared_ptr<CharacterSkin> detail::CharacterSkinFactory::Create(
    CharacterSkinStatus status, std::function<std::optional<std::string>(const std::string&)> map,
    std::function<std::string(const std::string&)> sound, std::function<bool()> close) {
    if (!map || !sound || !close)
        throw std::invalid_argument("skin services incomplete");
    auto value = std::shared_ptr<CharacterSkin>(new CharacterSkin());
    value->state_->status = std::move(status);
    value->state_->map = std::move(map);
    value->state_->sound = std::move(sound);
    value->state_->close = std::move(close);
    return value;
}
}
