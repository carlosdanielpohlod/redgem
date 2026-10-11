#include <doctest/doctest.h>

#include "core/xml_writer.hpp"

using namespace redgem;

TEST_CASE("an element with nothing inside closes itself") {
  XmlElement element("uses-permission");
  element.attribute("android:name", "android.permission.INTERNET");
  CHECK(element.text() == "<uses-permission android:name=\"android.permission.INTERNET\"/>\n");
}

TEST_CASE("elements nest, indented, with their attributes in the order they were given") {
  XmlElement root("manifest");
  root.attribute("package", "com.mystudio.game").attribute("version", "2");
  XmlElement& application = root.child("application");
  application.child("activity").attribute("name", "Main");
  application.child("service");
  CHECK(root.text() == "<manifest package=\"com.mystudio.game\" version=\"2\">\n"
                       "  <application>\n"
                       "    <activity name=\"Main\"/>\n"
                       "    <service/>\n"
                       "  </application>\n"
                       "</manifest>\n");
}

TEST_CASE("values may hold anything: what XML gives a meaning to is written out") {
  CHECK(xml_escaped("Tom & Jerry <3> \"quoted\"") == "Tom &amp; Jerry &lt;3&gt; &quot;quoted&quot;");
  CHECK(xml_escaped("plain") == "plain");
  XmlElement element("application");
  element.attribute("label", "A & B");
  CHECK(element.text() == "<application label=\"A &amp; B\"/>\n");
}

TEST_CASE("a document starts with the XML declaration") {
  const XmlElement root("manifest");
  CHECK(root.document() == "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<manifest/>\n");
}
