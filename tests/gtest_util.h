#pragma once

#define EXPECT_THROW_WITH_MESSAGE(stmt, etype, whatstring)                     \
  EXPECT_THROW(                                                                \
      try { stmt; } catch (const etype &ex) {                                  \
        EXPECT_EQ(whatstring, std::string(ex.what()));                         \
        throw;                                                                 \
      },                                                                       \
      etype)
