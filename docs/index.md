# Introduction

Extensible Virtual Display Interface (EVDI in short) is a software project that allows any userspace Linux program manage additional displays, and receive updates for them.
This short document explains how to use EVDI library in your application.

For more details about EVDI project and the latest code, see the [project page](https://github.com/DisplayLink/evdi) on DisplayLink's GitHub.

## Project origin

EVDI is a project that was started by DisplayLink as a base for the development of DisplayLink's Display driver for Ubuntu Linux, driving all
current generation, USB 3.0 Universal Docking Stations and USB Display Adapters.

However, it soon became apparent that we're making a generic interface that any other application could use - and that's when DisplayLink decided to open-source the code and push it to GitHub.

!!! info
    The interface of the library is still changing. Please treat the API as not yet stable - in terms of allowing the possibility that
	it can still be modified, and your clients may require matching updates.