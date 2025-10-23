/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include <ranges>
#define private public
#include "libcef/browser/net_database/cef_incognito_data_base_impl.h"
#undef private
#include "third_party/ohos_ndk/includes/ohos_adapter/ohos_adapter_helper.h"
class CefIncognitoDataBaseImplTest : public ::testing::Test {
protected:
    void SetUp() {
        db_.SetPermissionByOrigin("https://example.com", 
                                 CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                 true);
        db_.SetPermissionByOrigin("https://test.com", 
                                 CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                 true);
        db_.SetPermissionByOrigin("https://other.com", 
                                 CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                 false);
    }
    void TearDown() {
        db_.ClearAllPermission(CefIncognitoDataBaseImpl::GEOLOCATION_TYPE);
    }
    CefIncognitoDataBaseImpl db_;
};

TEST_F(CefIncognitoDataBaseImplTest, ExistPermissionByOrigin_001) {
    CefString emptyOrigin;
    bool result = db_.ExistPermissionByOrigin(emptyOrigin, CefIncognitoDataBaseImpl::GEOLOCATION_TYPE);
    EXPECT_FALSE(result);
}

TEST_F(CefIncognitoDataBaseImplTest, ExistPermissionByOrigin_002) {
    CefString origin("https://example.com");
    bool result = db_.ExistPermissionByOrigin(origin, 999);
    EXPECT_FALSE(result);
}

TEST_F(CefIncognitoDataBaseImplTest, ExistPermissionByOrigin_003) {
    CefString origin("https://example.com");
    bool result = db_.ExistPermissionByOrigin(origin, CefIncognitoDataBaseImpl::GEOLOCATION_TYPE);
    EXPECT_TRUE(result);
}

TEST_F(CefIncognitoDataBaseImplTest, ExistPermissionByOrigin_004) {
    CefString origin("https://nonexistent.com");
    bool result = db_.ExistPermissionByOrigin(origin, CefIncognitoDataBaseImpl::GEOLOCATION_TYPE);
    EXPECT_FALSE(result);
}

TEST_F(CefIncognitoDataBaseImplTest, GetPermissionResultByOrigin_001) {
    CefString emptyOrigin;
    bool result = true;
    bool returnValue = db_.GetPermissionResultByOrigin(emptyOrigin, 
                                                      CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                      result);
    
    EXPECT_FALSE(returnValue);
    EXPECT_TRUE(result);
}

TEST_F(CefIncognitoDataBaseImplTest, GetPermissionResultByOrigin_002) {
    CefString origin("https://example.com");
    bool result = true;
    bool returnValue = db_.GetPermissionResultByOrigin(origin, 999, result);
    
    EXPECT_FALSE(returnValue);
    EXPECT_TRUE(result);
}

TEST_F(CefIncognitoDataBaseImplTest, GetPermissionResultByOrigin_003) {
    CefString origin("https://example.com");
    bool result = false;
    bool returnValue = db_.GetPermissionResultByOrigin(origin, 
                                                      CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                      result);
    
    EXPECT_TRUE(returnValue);
    EXPECT_TRUE(result);
}

TEST_F(CefIncognitoDataBaseImplTest, GetPermissionResultByOrigin_004) {
    CefString origin("https://nonexistent.com");
    bool result = true;
    bool returnValue = db_.GetPermissionResultByOrigin(origin, 
                                                      CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                      result);
    
    EXPECT_FALSE(returnValue);
    EXPECT_TRUE(result);
}


TEST_F(CefIncognitoDataBaseImplTest, SetPermissionByOrigin_001) {
    CefString emptyOrigin;
    
    db_.SetPermissionByOrigin(emptyOrigin, 
                             CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                             true);
    
    bool result;
    bool found = db_.GetPermissionResultByOrigin(emptyOrigin, 
                                                CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                result);
    EXPECT_FALSE(found);
}

TEST_F(CefIncognitoDataBaseImplTest, SetPermissionByOrigin_002) {
    CefString origin("https://example1.com");
    int invalidType = 999;
    
    db_.SetPermissionByOrigin(origin, invalidType, true);
    
    bool result;
    bool found = db_.GetPermissionResultByOrigin(origin, 
                                                CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                result);
    EXPECT_FALSE(found);
}

TEST_F(CefIncognitoDataBaseImplTest, ClearPermissionByOrigin_001) {
    CefString emptyOrigin;
    
    db_.ClearPermissionByOrigin(emptyOrigin, CefIncognitoDataBaseImpl::GEOLOCATION_TYPE);
    
    bool result;
    bool found1 = db_.GetPermissionResultByOrigin("https://example.com", 
                                                 CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                 result);
    bool found2 = db_.GetPermissionResultByOrigin("https://test.com", 
                                                 CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                 result);
    bool found3 = db_.GetPermissionResultByOrigin("https://other.com", 
                                                 CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                 result);
    
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
    EXPECT_TRUE(found3);
}

TEST_F(CefIncognitoDataBaseImplTest, ClearPermissionByOrigin_002) {
    CefString origin("https://example.com");
    int invalidType = 999;
    
    db_.ClearPermissionByOrigin(origin, invalidType);
    
    bool result;
    bool found1 = db_.GetPermissionResultByOrigin("https://example.com", 
                                                 CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                 result);
    bool found2 = db_.GetPermissionResultByOrigin("https://test.com", 
                                                 CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                 result);
    bool found3 = db_.GetPermissionResultByOrigin("https://other.com", 
                                                 CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                 result);
    
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
    EXPECT_TRUE(found3);
}

TEST_F(CefIncognitoDataBaseImplTest, ClearPermissionByOrigin_003) {
    CefString origin("https://nonexistent.com");
    
    db_.ClearPermissionByOrigin(origin, CefIncognitoDataBaseImpl::GEOLOCATION_TYPE);
    
    bool result;
    bool found1 = db_.GetPermissionResultByOrigin("https://example.com", 
                                                 CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                 result);
    bool found2 = db_.GetPermissionResultByOrigin("https://test.com", 
                                                 CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                 result);
    bool found3 = db_.GetPermissionResultByOrigin("https://other.com", 
                                                 CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                 result);
    
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
    EXPECT_TRUE(found3);
}

TEST_F(CefIncognitoDataBaseImplTest, ClearPermissionByOrigin_004) {
    CefString origin("https://example.com");
    
    bool initialResult;
    bool initialFound = db_.GetPermissionResultByOrigin(origin, 
                                                       CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                       initialResult);
    EXPECT_TRUE(initialFound);
    
    db_.ClearPermissionByOrigin(origin, CefIncognitoDataBaseImpl::GEOLOCATION_TYPE);
    
    bool result;
    bool found = db_.GetPermissionResultByOrigin(origin, 
                                                CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                result);
    EXPECT_FALSE(found);
    
    bool found2 = db_.GetPermissionResultByOrigin("https://test.com", 
                                                 CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                 result);
    bool found3 = db_.GetPermissionResultByOrigin("https://other.com", 
                                                 CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                 result);
    
    EXPECT_TRUE(found2);
    EXPECT_TRUE(found3);
}

TEST_F(CefIncognitoDataBaseImplTest, ClearAllPermission_001) {
    db_.ClearAllPermission(1);
    
    bool result;
    bool found1 = db_.GetPermissionResultByOrigin("https://example.com", 
                                                 CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                 result);
    bool found2 = db_.GetPermissionResultByOrigin("https://test.com", 
                                                 CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                 result);
    bool found3 = db_.GetPermissionResultByOrigin("https://other.com", 
                                                 CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, 
                                                 result);
    
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
    EXPECT_TRUE(found3);
}

TEST_F(CefIncognitoDataBaseImplTest, GetOriginsByPermission_001) {
    int invalidType = 999;
    std::vector<CefString> origins;
    
    origins.push_back("pre-existing.com");
    
    db_.GetOriginsByPermission(invalidType, origins);
    
    EXPECT_EQ(1, origins.size());
    EXPECT_EQ("pre-existing.com", origins[0].ToString());
}

TEST_F(CefIncognitoDataBaseImplTest, GetOriginsByPermission_002) {
    db_.ClearAllPermission(CefIncognitoDataBaseImpl::GEOLOCATION_TYPE);
    
    std::vector<CefString> origins;
    
    origins.push_back("pre-existing.com");
    origins.push_back("another.com");
    
    db_.GetOriginsByPermission(CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, origins);
    
    EXPECT_TRUE(origins.empty());
}

TEST_F(CefIncognitoDataBaseImplTest, GetOriginsByPermission_003) {
    std::vector<CefString> origins;
    
    origins.push_back("pre-existing.com");
    
    db_.GetOriginsByPermission(CefIncognitoDataBaseImpl::GEOLOCATION_TYPE, origins);
    
    EXPECT_EQ(3, origins.size());
    
    bool foundExample = false;
    bool foundTest = false;
    bool foundOther = false;
    
    for (const auto& origin : origins) {
        std::string originStr = origin.ToString();
        if (originStr == "https://example.com") foundExample = true;
        if (originStr == "https://test.com") foundTest = true;
        if (originStr == "https://other.com") foundOther = true;
    }
    
    EXPECT_TRUE(foundExample);
    EXPECT_TRUE(foundTest);
    EXPECT_TRUE(foundOther);
}