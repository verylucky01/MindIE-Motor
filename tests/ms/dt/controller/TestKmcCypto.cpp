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
#include <gtest/gtest.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include "KmcSecureString.h"
#include "KmcDecryptor.h"

using namespace MINDIE::MS;

class TestKmcCrypto : public ::testing::Test {
protected:
    void SetUp() override {
    }
    void TearDown() override {
    }
};

// ------------------ KmcSecureString 功能测试 ------------------

TEST_F(TestKmcCrypto, SecureString_ValidAndClear)
{
    const char* data = "secret";
    KmcSecureString ss(data, 6); // 6：表示数据长度
    EXPECT_TRUE(ss.IsValid());
    EXPECT_STREQ(ss.GetSensitiveInfoContent(), data);
    EXPECT_EQ(ss.GetSensitiveInfoSize(), 6); // 6：表示数据长度

    ss.Clear();
    EXPECT_FALSE(ss.IsValid());
    EXPECT_EQ(ss.GetSensitiveInfoContent(), nullptr);
    EXPECT_EQ(ss.GetSensitiveInfoSize(), 0);
}

TEST_F(TestKmcCrypto, SecureString_InvalidInput)
{
    KmcSecureString ss(nullptr, -5); // -5:无效长度
    EXPECT_FALSE(ss.IsValid());
    EXPECT_EQ(ss.GetSensitiveInfoContent(), nullptr);
    EXPECT_EQ(ss.GetSensitiveInfoSize(), 0);
}

// ------------------ KmcDecryptor 功能测试 ------------------

// 派生类以访问 protected 方法
class TestableDecryptor : public KmcDecryptor {
public:
    TestableDecryptor() : KmcDecryptor(TlsItems()) {}

    // 测试 SerializeDecryptedKey
    KmcSecureString SerializeKey(EVP_PKEY* p)
    {
        return SerializeDecryptedKey(p);
    }
    // 测试 ParsePemPrivateKey
    KmcSecureString ParseKey(const std::unique_ptr<KmcSecureString>& kd,
        const std::unique_ptr<KmcSecureString>& pd)
    {
        return ParsePemPrivateKey(kd, pd);
    }
};

TEST_F(TestKmcCrypto, SerializeDecryptedKey_RoundTrip)
{
    // 生成 1024-bit RSA 密钥对
    BIGNUM* e = BN_new();
    ASSERT_NE(e, nullptr);
    BN_set_word(e, RSA_F4);

    RSA* rsa = RSA_new();
    ASSERT_NE(rsa, nullptr);
    ASSERT_EQ(RSA_generate_key_ex(rsa, 1024, e, nullptr), 1); // 1024：密钥长度 1: true

    EVP_PKEY* pkey = EVP_PKEY_new();
    ASSERT_NE(pkey, nullptr);
    EVP_PKEY_assign_RSA(pkey, rsa);

    TestableDecryptor dec;
    KmcSecureString pem = dec.SerializeKey(pkey);
    EXPECT_TRUE(pem.IsValid());
    EXPECT_GT(pem.GetSensitiveInfoSize(), 0);

    // 验证 PEM 可被 OpenSSL 重新解析
    std::unique_ptr<BIO, decltype(&BIO_free)> bio(
        BIO_new_mem_buf(pem.GetSensitiveInfoContent(), pem.GetSensitiveInfoSize()),
        BIO_free);
    ASSERT_NE(bio.get(), nullptr);

    EVP_PKEY* parsed = PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr);
    EXPECT_NE(parsed, nullptr);

    EVP_PKEY_free(parsed);
    EVP_PKEY_free(pkey);
    BN_free(e);
}

TEST_F(TestKmcCrypto, ParsePemPrivateKey_InvalidInputs)
{
    TestableDecryptor dec;

    // 空指针或无效 SecureString 应返回无效对象
    std::unique_ptr<KmcSecureString> nullK;
    std::unique_ptr<KmcSecureString> nullP;
    auto out1 = dec.ParseKey(nullK, nullP);
    EXPECT_FALSE(out1.IsValid());

    // 有 keyData 但无 passwordData
    const char* dummy = "abcd";
    auto kd = std::make_unique<KmcSecureString>(dummy, 4);
    EXPECT_TRUE(kd->IsValid());
    auto out2 = dec.ParseKey(kd, nullP);
    EXPECT_FALSE(out2.IsValid());

    // 有 passwordData 但无 keyData
    auto pd = std::make_unique<KmcSecureString>(dummy, 4);
    EXPECT_TRUE(pd->IsValid());
    auto out3 = dec.ParseKey(nullK, pd);
    EXPECT_FALSE(out3.IsValid());
}