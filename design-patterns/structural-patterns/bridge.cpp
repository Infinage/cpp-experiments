#include <iostream>

class Device {
    protected:
        bool power;
        int volume, channel;

    public:
        static constexpr int MIN_VOLUME {0}, MAX_VOLUME {100};
        static constexpr int MIN_CHANNEL {0}, MAX_CHANNEL {100};

        Device(): power(false), volume((MIN_VOLUME + MAX_VOLUME) / 2), channel(0) {}

        void enable()    { this->power = true; }
        void disable()   { this->power = false; }
        bool isEnabled() { return this->power; }

        int getVolume()  { return volume; }
        int getChannel() { return channel; }

        void setVolume(int volume)   { this->volume = std::max(MIN_VOLUME, std::min(volume, MAX_VOLUME)); }
        void setChannel(int channel) { this->channel = std::max(MIN_CHANNEL, std::min(channel, MAX_CHANNEL)); }

        virtual ~Device() = default;
        virtual void info() const = 0;
};

class Remote {
    protected:
        Device &device;
        
    public:
        static constexpr int VOLUME_DELTA {10}, CHANNEL_DELTA {1};

        Remote(Device &device): device(device) {}

        void togglePower() { device.isEnabled()? device.disable(): device.enable(); }

        void volumeUp()    { device.setVolume(device.getVolume() + VOLUME_DELTA); }
        void volumeDown()  { device.setVolume(device.getVolume() - VOLUME_DELTA); }

        void channelUp()   { device.setChannel(device.getChannel() + CHANNEL_DELTA); }
        void channelDown() { device.setChannel(device.getChannel() - CHANNEL_DELTA); }
};

// ---------------------- IMPLEMENTATIONS ---------------------- //

class Television: public Device {
    public:
        void info() const override {
            if (power)
                std::cout << "You are watching TV Channel #" << channel 
                          << ", Volume is set to " << volume << ".\n";
            else
                std::cout << "The TV is turned off.\n";
        }
};

class Radio: public Device {
    public:
        void info() const override {
            if (power)
                std::cout << "You are listening to Channel #" << channel 
                          << ", Volume is set to " << volume << ".\n";
            else
                std::cout << "The Radio is turned off.\n";
        }
};

class RemoteWithMute: public Remote {
    public:
        void mute() { device.setVolume(Device::MIN_VOLUME); }
};

// ---------------------- SAMPLE PROGRAM ---------------------- //

int main() {
    Television tv;
    Remote remote{tv};
    remote.togglePower();
    remote.channelUp();
    tv.info();

    Radio radio;
    RemoteWithMute advRemote{radio};
    advRemote.togglePower();
    advRemote.mute();
    radio.info();

    return 0;
}
