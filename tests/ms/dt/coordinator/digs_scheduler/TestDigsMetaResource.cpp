/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 * MindIE is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "gtest/gtest.h"
#include "coordinator/digs_scheduler/meta_resource.h"

const size_t ATTR_SLOTS = 0;
const size_t ATTR_BLOCKS = 1;

// 定义一个空 defaultAttr_ 的派生类
class EmptyDefaultAttrMetaResource : public MINDIE::MS::MetaResource {
public:
    using MINDIE::MS::MetaResource::MetaResource; // 继承构造函数
    static const std::vector<uint64_t> defaultAttr_; // 重定义静态成员变量
};

// 初始化静态成员变量
const std::vector<uint64_t> EmptyDefaultAttrMetaResource::defaultAttr_ = {};

class TestDigsMetaResource : public ::testing::Test {
protected:
    void SetUp() override {
        // 可以在这里初始化一些共用的数据或资源
        resource_.attributes_.clear();
    }

    void TearDown() override {
        // 可以在这里清理资源
    }
    MINDIE::MS::MetaResource resource_;
};

// MetaResource 测试用例，测试不含参数的构造函数
TEST_F(TestDigsMetaResource, MetaResourceTest001)
{
    MINDIE::MS::MetaResource metaResource;
    EXPECT_EQ(metaResource.attributes_, metaResource.defaultAttr_);
}

// 测试默认值初始化
TEST_F(TestDigsMetaResource, MetaResourceTest002)
{
    uint64_t defaultValue = 42;
    MINDIE::MS::MetaResource resource(defaultValue);

    // 验证所有属性都被正确初始化为 defaultValue
    for (auto attr : resource.attributes_) {
        EXPECT_EQ(attr, defaultValue);
    }

    // 验证 attributes_ 的大小与 defaultAttr_.size() 匹配
    EXPECT_EQ(resource.attributes_.size(), MINDIE::MS::MetaResource::defaultAttr_.size());
}

// CompareTo 测试用例，比较两个MetaResource对象是否相等
void CreateResourceWithAttribute(MINDIE::MS::MetaResource& resource, int attribute) {
    resource.attributes_.push_back(attribute);
}

TEST_F(TestDigsMetaResource, CompareToTest001)
{
    constexpr int attributeValueOne = 1;
    constexpr int attributeValueTow = 2;
    MINDIE::MS::MetaResource resource1;
    MINDIE::MS::MetaResource resource2;
    CreateResourceWithAttribute(resource1, attributeValueOne); // Set resource1 with attribute value 1
    EXPECT_FALSE(resource1.CompareTo(resource2));
    CreateResourceWithAttribute(resource1, attributeValueTow); // Set resource1 with attribute value 2
    CreateResourceWithAttribute(resource2, attributeValueOne); // Set resource2 with attribute value 1
    EXPECT_FALSE(resource1.CompareTo(resource2));
    CreateResourceWithAttribute(resource2, attributeValueTow); // Set resource2 with attribute value 2
    EXPECT_TRUE(resource1.CompareTo(resource2));
}

// InResource 测试用例，两个MetaResource对象累加
TEST_F(TestDigsMetaResource, InResourceTest001)
{
    constexpr int attributeValueForResourceOne = 1;
    constexpr int attributeValueForResourceTow = 2;

    MINDIE::MS::MetaResource resource1;
    MINDIE::MS::MetaResource resource2;
    CreateResourceWithAttribute(resource1, attributeValueForResourceOne); // 设置resource1的属性值为1
    EXPECT_FALSE(resource1.IncResource(resource2));  // 两个资源数量是否相等
    CreateResourceWithAttribute(resource2, attributeValueForResourceTow); // 设置resource2的属性值为2
    EXPECT_TRUE(resource1.IncResource(resource2));
    EXPECT_EQ(resource1.attributes_[0] + resource2.attributes_[0],
              attributeValueForResourceOne + attributeValueForResourceTow);
}

// DecResource 测试用例，两个MetaResource对象相减
TEST_F(TestDigsMetaResource, DecResourceTest001)
{
    constexpr int attributeResourceValueTow = 2;
    constexpr int attributeResourceValueOne = 1;

    MINDIE::MS::MetaResource resource1;
    MINDIE::MS::MetaResource resource2;
    CreateResourceWithAttribute(resource1, attributeResourceValueTow);  // 设置resource1的属性值为2
    EXPECT_FALSE(resource1.DecResource(resource2));  // 两个资源数量是否相等
    CreateResourceWithAttribute(resource2, attributeResourceValueOne);  // 设置resource2的属性值为1
    EXPECT_TRUE(resource1.DecResource(resource2));  // 两个资源相减
    EXPECT_EQ(resource2.attributes_[0] - resource1.attributes_[0],  attributeResourceValueOne);
}

// Slots() 测试用例，测试Slots()返回值是否正常
TEST_F(TestDigsMetaResource, SlotsTest001)
{
    constexpr int defaultSlotCountValue = 10;
    MINDIE::MS::MetaResource resource;
    resource.attributes_[ATTR_SLOTS] = defaultSlotCountValue;
    EXPECT_EQ(resource.Slots(), defaultSlotCountValue);
}

// Blocks() 测试用例，Blocks()返回值是否正常
TEST_F(TestDigsMetaResource, BlocksTest001)
{
    constexpr int defaultBlocksCountValue = 10;
    MINDIE::MS::MetaResource resource;
    resource.attributes_[ATTR_BLOCKS] = defaultBlocksCountValue;
    EXPECT_EQ(resource.Blocks(), defaultBlocksCountValue);
}

// UpdateBlocks() 测试用例
TEST_F(TestDigsMetaResource, UpdateBlocksTest001)
{
    MINDIE::MS::MetaResource resource;
    uint64_t previousValue = 10;
    uint64_t newValue = 20;
    resource.attributes_[ATTR_BLOCKS] = previousValue;
    EXPECT_EQ(previousValue, resource.UpdateBlocks(newValue));
    resource.UpdateBlocks(newValue);
    EXPECT_EQ(newValue, resource.attributes_[ATTR_BLOCKS]);
}

// InitAttrs() 测试用例,测试names、attrs分别为空和不为空的组合情况下的输出
TEST_F(TestDigsMetaResource, InitAttrsTest001)
{
    std::string names1 = "";
    std::string names2 = "name1,name2,name3";
    std::string attrs1 = "";
    std::string attrs2 = "1,2,3";
    std::string attrs3 = "1,invalid,3";
    std::string weights = "0.0034, 1, 0, 24, 6, 0.95, 0, 0";
    constexpr int attributeValueForInitttrsOne = 2;
    constexpr int attributeValueForInitttrsTow = 8;
    MINDIE::MS::MetaResource metaResource;
    metaResource.InitAttrs(names1, attrs2, weights);     // names为空，attrs不为空
    EXPECT_EQ(metaResource.attrName_.size(), attributeValueForInitttrsOne);

    metaResource.InitAttrs(names2, attrs1, weights);     // names不为空，attrs为空
    EXPECT_EQ(metaResource.defaultAttr_.size(), attributeValueForInitttrsOne);

    metaResource.InitAttrs(names2, attrs2, weights);     // names不为空，attrs不为空
    EXPECT_EQ(metaResource.attrName_.size(), attributeValueForInitttrsTow);
    EXPECT_EQ(metaResource.defaultAttr_.size(), attributeValueForInitttrsOne);

    metaResource.InitAttrs(names2, attrs3, weights);
    EXPECT_EQ(metaResource.defaultAttr_.size(), attributeValueForInitttrsOne);
}

// TotalLoad()
TEST_F(TestDigsMetaResource, TotalLoad_ShouldReturnCorrectSum_WhenAttributesArePositive)
{
    std::unique_ptr<MINDIE::MS::MetaResource> res = std::make_unique<MINDIE::MS::MetaResource>();
    res->attributes_ = {1, 2, 3, 4, 5};
    constexpr int attributeValueForTotalLoad = 15;
    uint64_t result = MINDIE::MS::MetaResource::TotalLoad(res);

    EXPECT_EQ(result, attributeValueForTotalLoad);
}

TEST_F(TestDigsMetaResource, TotalLoad_ShouldReturnZero_WhenNoAttributes)
{
    std::unique_ptr<MINDIE::MS::MetaResource> res = std::make_unique<MINDIE::MS::MetaResource>();
    res->attributes_ = {};
    constexpr int attributeValueForTotalLoad = 0;
    uint64_t result = MINDIE::MS::MetaResource::TotalLoad(res);

    EXPECT_EQ(result, attributeValueForTotalLoad);
}