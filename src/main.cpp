#include <Geode/Geode.hpp>
#include <Geode/modify/CCMouseDispatcher.hpp>

using namespace geode::prelude;

static double s_bufferedScrollY = 0;
static double s_bufferedScrollX = 0;
static bool s_isScrolling = false;
static bool s_emulateScroll = false;

static double getSpeed() {
    static double speed = (listenForSettingChanges<double>("scroll-speed", [](double value) {
        speed = value;
    }), Mod::get()->getSettingValue<double>("scroll-speed"));
    return speed;
}

static double getSensitivity() {
    static double speed = (listenForSettingChanges<double>("scroll-sensitivity", [](double value) {
        speed = value;
    }), Mod::get()->getSettingValue<double>("scroll-sensitivity"));
    return speed;
}

// for some specific mod compatibility reasons
static bool shouldPassthrough() {
    static auto volumeControls = Loader::get()->getLoadedMod("hjfod.quick-volume-controls") != nullptr;
    if (volumeControls) {
        return CCKeyboardDispatcher::get()->getAltKeyPressed();
    }
    return false;
}

class $modify(ScrolledCCMouseDispatcher, CCMouseDispatcher) {
    static void onModify(auto& self) {
        // ideally we want other hooks to not trigger on the initial call
        (void) self.setHookPriority("cocos2d::CCMouseDispatcher::dispatchScrollMSG", Priority::Early);
    }

    void updateScroll(float dt) {
        // scroll y
        auto bufferedY = s_bufferedScrollY;
        auto lerpedY = bufferedY * dt * getSpeed();
        s_bufferedScrollY -= lerpedY;

        // scroll x
        auto bufferedX = s_bufferedScrollX;
        auto lerpedX = bufferedX * dt * getSpeed();
        s_bufferedScrollX -= lerpedX;

        // if we're really close to 0, just stop
        if (std::abs(bufferedY) < 0.1f) {
            lerpedY = bufferedY;
            s_bufferedScrollY = 0;
        }
        if (std::abs(bufferedX) < 0.1f) {
            lerpedX = bufferedX;
            s_bufferedScrollX = 0;
        }

        if (s_bufferedScrollY == 0 && s_bufferedScrollX == 0) {
            this->stopSchedule();
        }

        s_emulateScroll = true;
        this->dispatchScrollMSG(lerpedY, lerpedX);
        s_emulateScroll = false;
    }

    void startSchedule() {
        if (s_isScrolling) return;
        s_isScrolling = true;
        CCScheduler::get()->scheduleSelector(
            schedule_selector(ScrolledCCMouseDispatcher::updateScroll),
            this,
            0,
            false
        );
    }

    void stopSchedule() {
        if (!s_isScrolling) return;
        s_isScrolling = false;
        CCScheduler::get()->unscheduleSelector(
            schedule_selector(ScrolledCCMouseDispatcher::updateScroll),
            this
        );
    }

    bool dispatchScrollMSG(float y, float x) {
        // bypass the scroll if we're emulating it, or there are no listeners
        if (s_emulateScroll || shouldPassthrough() || m_pMouseHandlers->count() == 0) {
            return CCMouseDispatcher::dispatchScrollMSG(y, x);
        }

        // don't smooth the scroll if we're in the level editor for now (zooming is a bit weird)
        if (LevelEditorLayer::get()) {
            return CCMouseDispatcher::dispatchScrollMSG(y, x);
        }

        // buffer the scroll amount
        s_bufferedScrollX += x * getSensitivity();
        s_bufferedScrollY += y * getSensitivity();
        this->startSchedule();
        return false;
    }
};

// a windows specific fix to enable horizontal scrolling, and also have proper fractional speed
#ifdef GEODE_IS_WINDOWS
#include <Geode/modify/CCEGLView.hpp>

class $modify(ScrolledCCEGLView, CCEGLView) {
    static void onModify(auto& self) {
        (void) self.setHookPriority("GLFWscrollfun", Priority::Replace);
    }

    void onGLFWMouseScrollCallback(GLFWwindow* window, double x, double y) {
        constexpr double SPEED = 12;
        CCDirector::get()->getMouseDispatcher()->dispatchScrollMSG(
            static_cast<float>(y * -SPEED),
            static_cast<float>(x * SPEED)
        );
    }
};
#endif