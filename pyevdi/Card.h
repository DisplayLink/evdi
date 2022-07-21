// Copyright (c) 2022 DisplayLink (UK) Ltd.
#ifndef CARD_H
#define CARD_H

class Card
{
    evdi_handle evdiHandle;
public:
    explicit Card(int device);
    ~Card();
    void close();
};

#endif
