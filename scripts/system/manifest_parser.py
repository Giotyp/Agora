import xml.etree.ElementTree as ET
import os
tree = ET.parse('manifest.xml')
root = tree.getroot()

node_info = []

for node in root:
    if node.tag == '{http://www.geni.net/resources/rspec/3}node':
        node_id = node.attrib['client_id']
        for vnode in node:
            if vnode.tag == '{http://www.protogeni.net/resources/rspec/ext/emulab/1}vnode':
                node_addr = vnode.attrib['name'] + ".utah.cloudlab.us"
                node_info.append((node_id, node_addr))
                break

f = open(os.path.expanduser('~/.ssh/config'), 'a')
for info in node_info:
    f.write('Host %s\n' % info[0])
    f.write('   HostName %s\n' % info[1])
    f.write('   User junzhig\n')
f.close()

f = open('node_list', 'w')
for info in node_info:
    f.write('%s\n' % info[0])
f.close()

f = open('addr_list', 'w')
for info in node_info:
    f.write('%s\n' % info[1])
f.close()