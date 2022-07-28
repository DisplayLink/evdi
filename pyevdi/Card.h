// Copyright (c) 2022 DisplayLink (UK) Ltd.
#ifndef CARD_H
#define CARD_H

class Card
{
    evdi_handle evdiHandle;
    evdi_event_context eventContext;
    evdi_mode mode;

public:
    std::function<void(struct evdi_mode)> m_modeHandler;

    explicit Card(int device);
    ~Card();
    void close();
    void connect(const char *edid, const unsigned int edid_length, 
        const uint32_t pixel_area_limit, const uint32_t pixel_per_second_limit);
    void disconnect();

    struct evdi_mode getMode() const;
    void setMode(struct evdi_mode mode);

    void handle_events(int waiting_time);
};

#endif
